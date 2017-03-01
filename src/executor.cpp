
#include "executor.h"

// LOOK HERE FIRST WHEN DEALING WITH CONCURRENCY BUGS

Executor Executor::singleton(std::thread::hardware_concurrency());

Executor::Executor(uint32_t num_threads) :
	n_threads(num_threads),
	thread_mask(BIT32(num_threads) - 1)
{
	running_threads = 0;
	if (n_threads > 1) {
		for (int i = 0; i < num_threads; ++i) {
			thread_pool.push_back(std::thread([=]() {(*this)(i);}));
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

void Executor::operator() (const int me) {
	const unsigned int flag = BIT8(me);
	const unsigned int antiflag = ~flag & thread_mask;
	while (true) {
		BatchFunc func;
		{
			std::unique_lock<std::mutex> lock(batch.mutex);
			// Wait for a batch job
			while ((running_threads & flag) == 0) batch.ready.wait(lock);

			func = batch.func;
			assert(func != nullptr);
		}

		if (batch.store_values) {
			for (int index = me; index < batch.n_items; index += n_threads) {
				void* data = reinterpret_cast<void*>(batch.data_buffer + index * batch.item_size);
				func(batch.share_data, data);
			}
		}
		else {
			for (int index = me; index < batch.n_items; index += n_threads) {
				void** data = reinterpret_cast<void**>(batch.data_buffer + index * batch.item_size);
				func(batch.share_data, *data);
			}
		}

		{
			std::unique_lock<std::mutex> lock(batch.mutex);
			running_threads &= antiflag;
			if (running_threads == 0) { // I am the last thread
				batch.done = true;
				batch.complete.notify_all();
			}
		}
	}
}

void Executor::run_batch() {
	assert(running_threads == 0);
	if (batch.func == nullptr || batch.n_items == 0) return;

	{
		std::unique_lock<std::mutex> lock(batch.mutex);

		batch.done = false;
		running_threads = thread_mask;
		batch.ready.notify_all();

		while (!batch.done) batch.complete.wait(lock);
	}

	batch.func = nullptr;
	batch.n_items = 0;
}

void Executor::run_deferred() {
	assert(running_threads == 0 && !deferred.running);
	deferred.running = true;
	for (auto& entry : deferred) {
		entry.func(entry.data);
	}
	deferred.running = false;
	deferred.mempool.clear();
	deferred.n_entries = 0;
}

void Executor::draw_begin() {
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
	drawing.n_entries = 0;
}

void Executor::draw_one(GPU_Target* screen) {
	assert(drawing.running && running_threads == 0 && !deferred.running);
	drawing.next->func(screen, drawing.next->data);
	drawing.next++;
}