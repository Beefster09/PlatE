#pragma once

#include "blockingconcurrentqueue.h"
#include "concurrentqueue.h"

#include <condition_variable>
#include <atomic>
#include <mutex>
#include <thread>

#include <vector>
#include <functional>

class Executor {
	typedef std::function<void()> Thunk;
	typedef moodycamel::BlockingConcurrentQueue<Thunk, moodycamel::ConcurrentQueueDefaultTraits> BlockingQueue;
	typedef moodycamel::ConcurrentQueue<Thunk, moodycamel::ConcurrentQueueDefaultTraits> Queue;
private:
	BlockingQueue immediate;
	Queue deferred;

	std::condition_variable complete; // triggers when async finishes
	std::mutex complete_mutex;

	std::atomic<int> n_immediate_thunks = 0;

	std::vector<std::thread> thread_pool;

	const bool single_threaded;

	void operator() ();
public:
	Executor(uint32_t num_threads);
	~Executor() {} // RAII has me covered

	Executor(const Executor& other) = delete;
	Executor(Executor&& other) = delete;

	/// Exec Thunk in another thread [eventually]
	void exec(const Thunk&);

	/// Wait for threads to run all the Thunks in the async queue
	void wait();

	/// Put Thunk in deferred queue to be run when you say so
	void defer(const Thunk&);

	/// Run all Thunks currently in the deferred queue (synchronous)
	void run_deferred();
};
