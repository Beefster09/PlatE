
#include "executor.h"

// LOOK HERE FIRST WHEN DEALING WITH CONCURRENCY BUGS

Executor Executor::singleton(std::thread::hardware_concurrency());

Executor::Executor(uint32_t num_threads) :
	single_threaded(num_threads <= 1)
{
	if (!single_threaded) {
		for (int i = 0; i < num_threads; ++i) {
			thread_pool.push_back(std::thread([&]() {(*this)(i+1);}));
			thread_pool[i].detach(); // The slave thread will never return, so detach it.
		}
	}
}

Executor::JobBatch::JobBatch() {
	func = nullptr;
}

Executor::DrawList::DrawList() :
	mempool(EXEC_DATA_BUFFER_SIZE) {}

Executor::MixedJobGroup::MixedJobGroup() :
	mempool(EXEC_DATA_BUFFER_SIZE) {}

Executor::~Executor() {
	operator delete ((void*) batch.data_buffer);
	delete[] drawing.entries;
	delete[] deferred.entries;
	drawing.mempool.free();
	deferred.mempool.free();
}

void Executor::operator() (int me) {
	while (true) {
		atomic_thread_fence(std::memory_order_release);
		{
			std::unique_lock<std::mutex> lock(batch.ready_m);
			while(!batch.running.load(std::memory_order_acquire)) batch.ready.wait(lock);
			running_threads++;
		}
		atomic_thread_fence(std::memory_order_acquire);
		BatchFunc func = batch.func;
		assert(func != nullptr);

		int index;
		if (batch.store_values) {
			while ((index = batch.item.fetch_add(1)) < batch.n_items) {
				void* data = reinterpret_cast<void*>(batch.data_buffer + index * batch.item_size);
				func(batch.share_data, data);
			}
		}
		else {
			while ((index = batch.item.fetch_add(1)) < batch.n_items) {
				void** data = reinterpret_cast<void**>(batch.data_buffer + index * batch.item_size);
				func(batch.share_data, *data);
			}
		}

		// at least one thread finished, so make sure we won't loop back around
		batch.running.store(false, std::memory_order_release);

		if (running_threads.fetch_sub(1) == 1) { // I am the last one
			batch.done.store(true, std::memory_order_release); // therefore all threads are done
			batch.complete.notify_all();
		}
	}
}

void Executor::run_batch() {
	assert(!batch.running);
	if (batch.func == nullptr || batch.n_items == 0) return;

	batch.item.store(0);

	batch.running = true;
	batch.done = false;
	atomic_thread_fence(std::memory_order_release);
	batch.ready.notify_all();

	{
		std::unique_lock<std::mutex> lock(batch.complete_m);
		while (!batch.done) batch.complete.wait(lock);
	}

	batch.func = nullptr;
	batch.n_items = 0;
}

void Executor::run_deferred() {
	atomic_thread_fence(std::memory_order_acquire);
	assert(!batch.running && !deferred.running);
	deferred.running = true;
	for (auto& entry : deferred) {
		entry.func(entry.data);
	}
	deferred.running = false;
	deferred.mempool.clear();
	deferred.n_entries.store(0);
}

void Executor::draw_begin() {
	atomic_thread_fence(std::memory_order_acquire);
#ifndef NDEBUG
	drawing.running = true;
#endif
	std::sort(drawing.begin(), drawing.end(),
		[](const DrawList::Entry& a, const DrawList::Entry& b) {
		return a.z_order < b.z_order;
	});
	drawing.next = drawing.begin();
}

void Executor::draw_end() {
#ifndef NDEBUG
	drawing.running = false;
#endif
	drawing.mempool.clear();
	drawing.n_entries.store(0);
}

void Executor::draw_one(GPU_Target* screen) {
	assert(drawing.running && !batch.running && !deferred.running);
	drawing.next->func(screen, drawing.next->data);
	drawing.next++;
}