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

namespace aion::chatserver::network::netty {

/**
 * Runs the packets of the connections (and their disconnect callbacks) on a thread pool, the tasks of one connection one at a time in the order
 * they were submitted, the tasks of different connections in parallel.
 * <p>
 * C++ replacement of Netty's DefaultEventExecutorGroup(8, new DefaultThreadFactory("PacketProcessor")) of NetConnector, whose next() executor
 * every connection gets. Deviation: Netty binds each connection to one of the 8 threads, so a slow packet also delays the connections sharing
 * its thread; here a connection is only bound to its own queue, and up to maxThreads threads ("PacketProcessor-&lt;n&gt;") are started on
 * demand, one for each connection that is ready while no idle thread is left to take it, and stay until shutdown(). The number of queued
 * packets per connection is limited by NettyConnection.
 * <p>
 * Must be owned by a std::shared_ptr (std::make_shared): every thread holds a reference until it ends, so a thread that shutdown() leaves behind
 * can still finish its task. Thread safe. Exceptions escaping a task are logged, the thread continues.
 */
class PacketExecutorGroup : public std::enable_shared_from_this<PacketExecutorGroup> {
public:
	using Task = std::function<void()>;

	/** @param maxThreads maximum number of threads (&gt; 0) */
	explicit PacketExecutorGroup(int32_t maxThreads);

	/**
	 * Calls shutdown() with a zero timeout. The threads hold references to the group, so it is only destroyed when none was started or after
	 * shutdown() stopped them; the owner must call shutdown() (NetConnector does).
	 */
	~PacketExecutorGroup();

	PacketExecutorGroup(const PacketExecutorGroup&) = delete;
	PacketExecutorGroup& operator=(const PacketExecutorGroup&) = delete;

	/**
	 * Queues a task of a channel. Tasks with the same channel key run one at a time in submission order. Tasks submitted after shutdown() are
	 * discarded.
	 *
	 * @param channel identifies the channel (its connection); only compared, never dereferenced
	 */
	void execute(const void* channel, Task task);

	/**
	 * Java: next() - an executor for one connection, which runs its tasks one after another in submission order (on any of the threads). It keeps
	 * this group alive.
	 */
	std::function<void(Task task)> next();

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

} // namespace aion::chatserver::network::netty
