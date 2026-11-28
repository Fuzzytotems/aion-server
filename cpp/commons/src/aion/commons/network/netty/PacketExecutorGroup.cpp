#include "aion/commons/network/netty/PacketExecutorGroup.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::commons::network::netty {

namespace {

const logging::Logger& log() {
	static const auto* logger = new logging::Logger(logging::LoggerFactory::getLogger("com.aionemu.commons.network.netty.PacketExecutorGroup"));
	return *logger;
}

} // namespace

PacketExecutorGroup::PacketExecutorGroup(int32_t maxThreads) : maxThreads(maxThreads) {
	if (maxThreads < 1)
		throw utils::IllegalArgumentException("maxThreads must be at least 1, but was " + std::to_string(maxThreads));
}

PacketExecutorGroup::~PacketExecutorGroup() {
	try {
		shutdown(std::chrono::milliseconds(0));
	} catch (...) {
		try {
			log().errorCurrentException("Error shutting down the execution handler");
		} catch (...) {
		}
	}
}

void PacketExecutorGroup::execute(const void* channel, Task task) {
	std::lock_guard lock(mutex);
	if (stopping)
		return; // the task is destroyed when this call returns
	ChannelQueue& queue = queues[channel];
	queue.tasks.push_back(std::move(task));
	pendingTasks++;
	if (queue.running || queue.tasks.size() > 1)
		return; // the thread running the channel's current task, or the ready list, takes care of it
	ready.push_back(channel);
	// each ready channel needs a thread of its own; a notified thread counts as idle until it took its channel, so the number of idle threads
	// decides, not whether there is one
	if (std::cmp_greater(ready.size(), idleThreads) && std::cmp_less(threads.size(), maxThreads)) {
		size_t index = threads.size();
		threads.emplace_back([self = shared_from_this(), index] {
			utils::concurrent::setCurrentThreadName("PacketProcessor-" + std::to_string(index));
			self->workerLoop(index);
		});
		exited.push_back(false);
		idleThreads++; // idle until it took a channel
		liveThreads++;
	} else {
		readyChanged.notify_one();
	}
}

std::function<void(PacketExecutorGroup::Task task)> PacketExecutorGroup::next() {
	auto key = std::make_shared<char>(); // its address identifies the connection while the executor lives
	return [self = shared_from_this(), key](Task task) { self->execute(key.get(), std::move(task)); };
}

void PacketExecutorGroup::workerLoop(size_t index) {
	std::unique_lock lock(mutex);
	for (;;) {
		readyChanged.wait(lock, [this] { return stopping || !ready.empty(); });
		idleThreads--;
		if (ready.empty())
			break; // stopping

		const void* channel = ready.front();
		ready.pop_front();
		ChannelQueue& queue = queues[channel];
		Task task = std::move(queue.tasks.front());
		queue.tasks.pop_front();
		queue.running = true;
		runningThreads++;
		lock.unlock();

		try {
			task();
		} catch (...) {
			try {
				log().errorCurrentException("Exception while executing a task");
			} catch (...) {
			}
		}
		task = nullptr; // releases what the task holds (e.g. its connection) outside the lock

		lock.lock();
		runningThreads--;
		pendingTasks--;
		auto current = queues.find(channel);
		current->second.running = false;
		if (current->second.tasks.empty()) {
			queues.erase(current);
		} else {
			ready.push_back(channel);
			readyChanged.notify_one();
		}
		idleThreads++;
		taskFinished.notify_all();
	}
	liveThreads--;
	exited[index] = true;
	threadExited.notify_all();
}

void PacketExecutorGroup::shutdown(std::chrono::milliseconds timeout) {
	std::vector<std::thread> toJoin;
	std::vector<bool> ended;
	std::vector<Task> discarded; // destroyed outside the lock
	{
		std::unique_lock lock(mutex);
		if (stopping)
			return;
		taskFinished.wait_for(lock, timeout, [this] { return pendingTasks == 0; });
		stopping = true;
		for (auto& [channel, queue] : queues) {
			for (Task& task : queue.tasks)
				discarded.push_back(std::move(task));
			pendingTasks -= queue.tasks.size();
			queue.tasks.clear();
		}
		ready.clear();
		readyChanged.notify_all();
		// the idle threads end at once; the ones still running a task are not waited for
		threadExited.wait(lock, [this] { return liveThreads == runningThreads; });
		toJoin = std::move(threads);
		threads.clear();
		ended = exited;
	}
	if (!discarded.empty())
		log().warn("Discarded {} tasks on shutdown", discarded.size());
	discarded.clear();
	size_t leftRunning = 0;
	for (size_t i = 0; i < toJoin.size(); i++) {
		if (ended[i]) {
			toJoin[i].join();
		} else {
			toJoin[i].detach(); // it holds a reference to this handler and ends after its task
			leftRunning++;
		}
	}
	if (leftRunning > 0)
		log().warn("{} threads still run a task after the shutdown timeout, they end after it", leftRunning);
}

size_t PacketExecutorGroup::getPendingTaskCount() const {
	std::lock_guard lock(mutex);
	return pendingTasks;
}

size_t PacketExecutorGroup::getChannelCount() const {
	std::lock_guard lock(mutex);
	return queues.size();
}

} // namespace aion::commons::network::netty
