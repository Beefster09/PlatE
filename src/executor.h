#pragma once

#include "mempool.h"
#include "error.h"
#include "result.h"

#include <condition_variable>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

#include "SDL_gpu.h"

namespace Errors {
	const error_data
		TooManyDeferredCalls = { 70, "Executor has reached its limit on number of calls" },
		TooManyDeferredDraws = { 71, "Executor has reached its limit on number of draw calls" };
}

constexpr size_t EXEC_DATA_BUFFER_SIZE = 1024 * 128; // 128 KiB - more than you should ever need
constexpr size_t MAX_DEFERRED_CALLS = 8192;
constexpr size_t MAX_DEFERRED_DRAWS = 4096;
constexpr size_t SHARED_DATA_MAXSIZE = 256;

// An execution context for master and slave threads to communicate
// The master thread has to make all the SDL calls and manages things that MUST be run in sequence
// Slaves do physics and scripting and other things where only reading is required or where they have non-overlapping data
// The master and slaves should never be running at the same time.
class Executor {
private:
	// One producer (master), Many consumers (slaves)
	typedef void(*BatchFunc)(const void*, void*);
	struct JobBatch {
		BatchFunc func;
		void* share_data = operator new(SHARED_DATA_MAXSIZE);

		intptr_t const data_buffer = reinterpret_cast<intptr_t>(operator new (EXEC_DATA_BUFFER_SIZE));
		size_t item_size;
		std::atomic<int> item; // Index into the data array
		int n_items = 0;
		bool store_values = false;

		std::condition_variable complete; // slave -> master signal
		std::mutex complete_m;
		std::condition_variable ready;    // master -> slave signal
		std::mutex ready_m;
		volatile bool running = false;

		JobBatch();
	} batch;

	// Many producers (slaves), One consumer (master)
	typedef void(*DrawFunc)(GPU_Target*, void*);
	struct DrawList {
		struct Entry {
			DrawFunc func;
			void* data;
			int z_order;
		};
		Entry* const entries = new Entry[MAX_DEFERRED_DRAWS];
		std::atomic<int> n_entries;
		const Entry* next; // used by master thread for iterating
		AtomicMemoryPool mempool;

		DrawList();
		inline const Entry* begin() const { return entries; }
		inline const Entry* end() const { return entries + n_entries; }
		inline Entry* begin() { return entries; }
		inline Entry* end() { return entries + n_entries; }
	} drawing;

	// Many producers (slaves), One consumer (master)
	typedef void(*SingleFunc)(void*);
	struct MixedJobGroup {
		struct Entry {
			SingleFunc func;
			void* data;
		};
		Entry* const entries = new Entry[MAX_DEFERRED_CALLS];
		std::atomic<int> n_entries;
		AtomicMemoryPool mempool;

		MixedJobGroup();
		inline const Entry* begin() const { return entries; }
		inline const Entry* end() const { return entries + n_entries; }
		inline Entry* begin() { return entries; }
		inline Entry* end() { return entries + n_entries; }
	} deferred;

	std::vector<std::thread> thread_pool;
	std::atomic<int> running_threads;
	const bool single_threaded;

	void operator() ();

	Executor(uint32_t num_threads);
	~Executor();

public:
	Executor(const Executor& other) = delete;
	Executor(Executor&& other) = delete;

	static Executor singleton;

	template<typename SharedT, typename ItemT>
	void set_batch_job(void(*func)(const SharedT*, ItemT*), const SharedT& share_data, bool byValue = true) {
		static_assert(sizeof(SharedT) <= SHARED_DATA_MAXSIZE, "Shared data too large");
		assert(!batch.running);
		batch.func = reinterpret_cast<BatchFunc>(func);
		*reinterpret_cast<SharedT*>(batch.share_data) = share_data;
		batch.store_values = byValue;
		if (byValue) {
			batch.item_size = sizeof(ItemT);
		}
		else { // store pointers instead of values
			batch.item_size = sizeof(ItemT*);
		}
		batch.n_items = 0;
	}

	// It's assumed that if you don't specify a type and give a struct that you won't use it.
	template<typename ItemT>
	void set_batch_job(void(*func)(const void*, ItemT*), bool byValue = true) {
		assert(!batch.running);
		batch.func = reinterpret_cast<BatchFunc>(func);
		batch.store_values = byValue;
		if (byValue) {
			batch.item_size = sizeof(ItemT);
		}
		else { // store literal pointers
			batch.item_size = sizeof(ItemT*);
		}
		batch.n_items = 0;
	}

	/// Submit an item to the batch
	template<typename T>
	void submit(const T& data) {
		assert(!batch.running);
		assert(sizeof(T) == batch.item_size);
		reinterpret_cast<T*>(batch.data_buffer)[batch.n_items++] = data;
	}

	/// Run the batch and wait for it to complete
	void run_batch();

	/// Add a call to the deferred group
	template<typename T>
	Result<> defer(void(*func)(T*), T& data) {
		int index = deferred.n_entries.fetch_add(1);
		if (index >= MAX_DEFERRED_CALLS) {
			return Errors::TooManyDeferredCalls;
		}
		T* dptr = deferred.mempool.alloc<T>();
		if (dptr == nullptr) {
			return Errors::BadAlloc;
		}
		*dptr = data;
		deferred.entries[index] = MixedJobGroup::Entry{
			reinterpret_cast<SingleFunc>(func),
			dptr
		};
		return Result<>::success;
	}

	/// Run all calls currently in the deferred queue
	void run_deferred();

	/// Add a drawing call to the drawing list
	template<typename T>
	void defer_draw(void(*func)(GPU_Target*, T*), T& data, int z_order) {
		int index = drawing.n_entries.fetch_add(1);
		if (index >= MAX_DEFERRED_DRAWS) {
			return Errors::TooManyDeferredDraws;
		}
		T* dptr = mempool.alloc<T>();
		if (dptr == nullptr) {
			return Errors::BadAlloc;
		}
		*dptr = data;
		drawing.entries[index] = DrawList::Entry{
			reinterpret_cast<DrawFunc>(func),
			dptr,
			z_order
		};
		return Result<>::success;
	}

	inline void sort_draw_list() {
		std::sort(drawing.begin(), drawing.end(),
			[](const DrawList::Entry& a, const DrawList::Entry& b) {
				return a.z_order < b.z_order;
		});
		drawing.next = drawing.begin();
	}
	inline int peek_draw_z() {
		return drawing.next->z_order;
	}
	void draw_one(GPU_Target*);

	void draw_clear();
};

// This reduces typing and makes transitioning over to the new system more seamless.
#define executor Executor::singleton