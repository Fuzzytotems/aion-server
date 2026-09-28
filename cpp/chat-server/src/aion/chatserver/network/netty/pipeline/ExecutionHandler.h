#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace aion::chatserver::network::netty::pipeline {

/**
 * Runs the upstream events of the client channels (connected, message received, disconnected) on a thread pool, the events of one channel one
 * at a time in the order they were submitted, the events of different channels in parallel.
 * <p>
 * C++ replacement of Netty's org.jboss.netty.handler.execution.ExecutionHandler with an OrderedMemoryAwareThreadPoolExecutor, which
 * LoginToClientPipeLineFactory puts in front of the ClientChannelHandler. Up to maxThreads threads ("ExecutionHandler-&lt;n&gt;") are started
 * on demand, one for each channel that is ready while no idle thread is left to take it (Java's ThreadPoolExecutor starts a thread for each task
 * below its core size), and stay until shutdown(). Deviation: the executor's memory limits (1 MB per channel, 128 MB in total, beyond which Netty
 * blocks the IO thread) are not enforced; the queues are unbounded like the send queues of all connections.
 * <p>
 * Must be owned by a std::shared_ptr (std::make_shared): every thread holds a reference until it ends, so a thread that shutdown() leaves behind
 * can still finish its task. Thread safe. Exceptions escaping a task are logged, the thread continues.
 */
class ExecutionHandler : public std::enable_shared_from_this<ExecutionHandler> {
public:
	using Task = std::function<void()>;

	/** @param maxThreads maximum number of threads (&gt; 0) */
	explicit ExecutionHandler(int32_t maxThreads);

	/**
	 * Calls shutdown() with a zero timeout. The threads hold references to the handler, so it is only destroyed when none was started or after
	 * shutdown() stopped them; the owner must call shutdown() (LoginToClientPipeLineFactory does).
	 */
	~ExecutionHandler();

	ExecutionHandler(const ExecutionHandler&) = delete;
	ExecutionHandler& operator=(const ExecutionHandler&) = delete;

	/**
	 * Queues a task of a channel. Tasks with the same channel key run one at a time in submission order. Tasks submitted after shutdown() are
	 * discarded.
	 *
	 * @param channel identifies the channel (its connection); only compared, never dereferenced
	 */
	void execute(const void* channel, Task task);

	/**
	 * Waits up to timeout until all queued and running tasks ran, then discards the queued ones that are left and stops the threads: the idle ones
	 * are joined, a thread still running a task (e.g. a chat log insert on a database that does not answer) is detached and ends after its task.
	 * Calling it again does nothing. Called from a task, it detaches that task's thread.
	 */
	void shutdown(std::chrono::milliseconds timeout);

	/** @return number of tasks queued or running */
	size_t getPendingTaskCount() const;

	/** C++ addition for tests: number of channels with queued or running tasks (a channel is forgotten when its last task ended) */
	size_t getChannelCount() const;

private:
	struct ChannelQueue {
		std::deque<Task> tasks;
		/** true while a thread runs a task of this channel */
		bool running = false;
	};

	/** @param index the thread's index in threads and exited */
	void workerLoop(size_t index);

	const int32_t maxThreads;

	mutable std::mutex mutex;
	/** notified when a channel becomes ready or on shutdown */
	std::condition_variable readyChanged;
	/** notified when a task finished */
	std::condition_variable taskFinished;
	/** notified when a thread ended its loop */
	std::condition_variable threadExited;
	std::unordered_map<const void*, ChannelQueue> queues;
	/** channels with queued tasks and no running task, in the order they became ready */
	std::deque<const void*> ready;
	size_t pendingTasks = 0;
	/** threads waiting for a ready channel, including started and notified threads that did not take one yet */
	int32_t idleThreads = 0;
	/** threads running a task */
	int32_t runningThreads = 0;
	/** threads whose loop did not end yet */
	int32_t liveThreads = 0;
	bool stopping = false;
	std::vector<std::thread> threads;
	/** per thread (same index as in threads): true once its loop ended */
	std::vector<bool> exited;
};

} // namespace aion::chatserver::network::netty::pipeline
