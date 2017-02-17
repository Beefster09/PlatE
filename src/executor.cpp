
#include "executor.h"

Executor Executor::singleton(std::thread::hardware_concurrency());

Executor::Executor(uint32_t num_threads) :
	single_threaded(num_threads <= 1)
{
	if (!single_threaded) {
		for (int i = 0; i < num_threads; ++i) {
			thread_pool.push_back(std::thread([&]() {(*this)();}));
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

void Executor::operator() () {
	while (true) {
		{
			std::unique_lock<std::mutex> lock(batch.ready_m);
			batch.ready.wait(lock, [this] { return batch.running; });
		}
		assert(batch.func != nullptr);

		running_threads++;

		int index;
		if (batch.store_values) {
			while ((index = batch.item.fetch_add(1)) < batch.n_items) {
				void* data = reinterpret_cast<void*>(batch.data_buffer + index * batch.item_size);
				batch.func(batch.share_data, data);
			}
		}
		else {
			while ((index = batch.item.fetch_add(1)) < batch.n_items) {
				void** data = reinterpret_cast<void**>(batch.data_buffer + index * batch.item_size);
				batch.func(batch.share_data, *data);
			}
		}

		if (running_threads.fetch_sub(1) == 1) { // I am the last one
			batch.running = false;
			batch.complete.notify_all();
		}
	}
}

void Executor::run_batch() {
	assert(!batch.running);
	if (batch.func == nullptr || batch.n_items == 0) return;

	batch.item.store(0);

	batch.running = true;
	batch.ready.notify_all();

	{
		std::unique_lock<std::mutex> lock(batch.complete_m);
		batch.complete.wait(lock, [this] { return !batch.running; });
	}

	batch.func = nullptr;
	batch.n_items = 0;
}

void Executor::run_deferred() {
	assert(!batch.running);
	for (auto& entry : deferred) {
		entry.func(entry.data);
	}
	deferred.mempool.clear();
	deferred.n_entries.store(0);
}

void Executor::draw_clear() {
	drawing.mempool.clear();
	drawing.n_entries.store(0);
}

void Executor::draw_one(GPU_Target* screen) {
	drawing.next->func(screen, drawing.next->data);
	drawing.next++;
}