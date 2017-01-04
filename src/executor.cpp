
#include "executor.h"

constexpr uint32_t ExecQueueInitSize = 256;

Executor::Executor(uint32_t num_threads) :
	immediate(num_threads > 1? ExecQueueInitSize: 0),
	deferred(ExecQueueInitSize),
	single_threaded(num_threads <= 1)
{
	if (!single_threaded) {
		for (int i = 0; i < num_threads; ++i) {
			thread_pool.push_back(std::thread([&]() {(*this)();}));
		}
	}
}

void Executor::operator() () {
	while (true) {
		Thunk thunk;

		immediate.wait_dequeue(thunk);

		if (thunk) {
			thunk();
		}
		if (--n_immediate_thunks == 0) {
			complete.notify_all();
		}
	}
}

void Executor::exec(const Thunk& thunk) {
	if (single_threaded) {
		thunk();
	}
	else {
		immediate.enqueue(thunk);
		++n_immediate_thunks;
	}
}

void Executor::wait() {
	if (single_threaded) return;

	std::unique_lock<std::mutex> lock(complete_mutex);
	while (n_immediate_thunks > 0) {
		complete.wait(lock);
	}
}

void Executor::defer(const Thunk& thunk) {
	deferred.enqueue(thunk);
}

void Executor::run_deferred() {
	Thunk thunk;
	while (deferred.try_dequeue(thunk)) {
		//printf("Running deferred thunk...\n");
		if (thunk) {
			thunk();
		}
	}
}