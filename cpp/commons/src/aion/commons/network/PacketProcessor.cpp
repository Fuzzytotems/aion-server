#include "aion/commons/network/PacketProcessor.h"

#include <algorithm>
#include <cstdint>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::commons::network {

namespace {

// Intentionally leaked and created on first use, so the network classes can be constructed and destroyed as statics (Java: static fields) without
// depending on the initialization or destruction order of namespace scope statics.
const logging::Logger& log() {
	static const auto* logger = new logging::Logger(logging::LoggerFactory::getLogger("com.aionemu.commons.network.PacketProcessor"));
	return *logger;
}

void checkArgument(bool condition, const char* errorMessage) {
	if (!condition)
		throw utils::IllegalArgumentException(errorMessage);
}

} // namespace

PacketProcessorBase::PacketProcessorBase(int32_t minThreads, int32_t maxThreads, int32_t threadSpawnThreshold, int32_t threadKillThreshold,
	Executor executor)
	: minThreads(minThreads), maxThreads(maxThreads), threadSpawnThreshold(threadSpawnThreshold), threadKillThreshold(threadKillThreshold),
		executor(std::move(executor)) {
	checkArgument(minThreads > 0, "Min Threads must be positive");
	checkArgument(maxThreads >= minThreads, "Max Threads must be >= Min Threads");
	checkArgument(threadSpawnThreshold > 0, "Thread Spawn Threshold must be positive");
	checkArgument(threadKillThreshold > 0, "Thread Kill Threshold must be positive");

	try {
		{
			std::lock_guard lock(mutex);
			for (int32_t i = 0; i < minThreads; i++)
				newThread();
		}
		if (minThreads != maxThreads)
			checkerThread = std::thread([this] { checkerLoop(); });
	} catch (...) {
		shutdown(); // don't leave joinable threads behind
		throw;
	}
}

PacketProcessorBase::~PacketProcessorBase() {
	shutdown();
	// if shutdown was called by a packet, its worker thread was detached: wait until it has left workerLoop
	std::unique_lock lock(mutex);
	workerFinished.wait(lock, [this] { return std::ranges::all_of(threads, [](const auto& worker) { return worker->finished; }); });
}

bool PacketProcessorBase::newThread() {
	if (static_cast<int32_t>(threads.size()) >= maxThreads)
		return false;

	auto worker = std::make_unique<Worker>();
	worker->name = "PacketProcessor:" + std::to_string(threads.size());
	log().debug("Creating new PacketProcessor Thread: " + worker->name);

	Worker& workerRef = *worker;
	threads.push_back(std::move(worker));
	try {
		workerRef.thread = std::thread([this, &workerRef] { workerLoop(workerRef); });
	} catch (...) {
		threads.pop_back();
		throw;
	}
	return true;
}

void PacketProcessorBase::killThread() {
	if (static_cast<int32_t>(threads.size()) > minThreads) {
		std::unique_ptr<Worker> worker = std::move(threads.back());
		threads.pop_back();
		log().debug("Killing PacketProcessor Thread: " + worker->name);
		worker->killed = true;
		killedThreads.push_back(std::move(worker));
		notEmpty.notify_all();
	}
}

void PacketProcessorBase::joinFinishedThreads() {
	std::erase_if(killedThreads, [](const std::unique_ptr<Worker>& worker) {
		if (!worker->finished)
			return false;
		worker->thread.join(); // the thread has released the lock and is about to return
		return true;
	});
}

void PacketProcessorBase::execute(const void* connectionKey, std::unique_ptr<packet::ClientPacketBase> packet) {
	std::unique_lock lock(mutex);
	if (shuttingDown)
		return; // the packet is discarded (destroyed with the parameter, after the lock was released)

	ConnectionQueue& queue = queues[connectionKey];
	queue.packets.push_back(std::move(packet));
	waitingPackets++;
	if (!queue.running && queue.packets.size() == 1) {
		readyConnections.push_back(connectionKey);
		notEmpty.notify_one();
	}
}

void PacketProcessorBase::workerLoop(Worker& worker) {
	utils::concurrent::setCurrentThreadName(worker.name);
	std::unique_lock lock(mutex);
	for (;;) {
		notEmpty.wait(lock, [&] { return worker.killed || shuttingDown || !readyConnections.empty(); });
		if (worker.killed || shuttingDown) {
			worker.finished = true;
			workerFinished.notify_all();
			// this thread may have consumed a notification meant for a thread that can take the packet
			if (!readyConnections.empty())
				notEmpty.notify_one();
			return;
		}

		const void* connectionKey = readyConnections.front();
		readyConnections.pop_front();
		ConnectionQueue& queue = queues.at(connectionKey); // references to unordered_map elements stay valid until the element is erased
		std::unique_ptr<packet::ClientPacketBase> packet = std::move(queue.packets.front());
		queue.packets.pop_front();
		queue.running = true;
		waitingPackets--;

		lock.unlock();
		runPacket(*packet);
		packet.reset(); // may release the last reference to the connection, so destroy it without holding the lock
		lock.lock();

		queue.running = false;
		if (queue.packets.empty()) {
			queues.erase(connectionKey);
		} else {
			readyConnections.push_back(connectionKey);
			notEmpty.notify_one();
		}
	}
}

void PacketProcessorBase::runPacket(packet::ClientPacketBase& packet) const {
	try {
		if (executor)
			executor(packet);
		else
			packet.run();
	} catch (...) {
		try {
			log().errorCurrentException("Unhandled exception while executing client packet " + packet.toString());
		} catch (...) {
		}
	}
}

void PacketProcessorBase::checkerLoop() {
	utils::concurrent::setCurrentThreadName("PacketProcessor:Checker");
	for (;;) {
		{
			std::unique_lock lock(mutex);
			if (shutdownRequested.wait_for(lock, CHECK_INTERVAL, [this] { return shuttingDown; }))
				return;
		}
		try {
			checkThreadCount();
		} catch (...) {
			try {
				log().errorCurrentException("");
			} catch (...) {
			}
		}
	}
}

void PacketProcessorBase::checkThreadCount() {
	std::lock_guard lock(mutex);
	if (shuttingDown)
		return;
	joinFinishedThreads();

	int32_t packetsWaitingForExecution = waitingPackets;
	if (packetsWaitingForExecution <= previousPacketCount && packetsWaitingForExecution <= threadKillThreshold) {
		// reduce thread count by one
		killThread();
	} else if (packetsWaitingForExecution > threadSpawnThreshold) {
		// too small amount of threads
		if (!newThread() && packetsWaitingForExecution >= static_cast<int64_t>(threadSpawnThreshold) * 3)
			log().warn("Lag detected! [" + std::to_string(packetsWaitingForExecution) +
							 " client packets are waiting for execution]. You should consider increasing PacketProcessor maxThreads or hardware upgrade.");
	}
	previousPacketCount = packetsWaitingForExecution;
}

int32_t PacketProcessorBase::getThreadCount() const {
	std::lock_guard lock(mutex);
	return static_cast<int32_t>(threads.size());
}

int32_t PacketProcessorBase::getWaitingPacketCount() const {
	std::lock_guard lock(mutex);
	return waitingPackets;
}

void PacketProcessorBase::shutdown() {
	std::vector<std::unique_ptr<Worker>> workers;
	{
		std::lock_guard lock(mutex);
		if (shuttingDown)
			return;
		shuttingDown = true;
		workers = std::move(threads);
		threads.clear();
		for (auto& worker : killedThreads)
			workers.push_back(std::move(worker));
		killedThreads.clear();
	}
	notEmpty.notify_all();
	shutdownRequested.notify_all();

	auto currentThread = std::this_thread::get_id();
	bool calledByWorker = false;
	auto joinOrDetach = [&](std::thread& thread) {
		if (!thread.joinable())
			return;
		if (thread.get_id() == currentThread) {
			// shutdown called while executing a packet: this thread exits after the packet
			thread.detach();
			calledByWorker = true;
		} else {
			thread.join();
		}
	};
	if (checkerThread.joinable())
		joinOrDetach(checkerThread);
	for (auto& worker : workers)
		joinOrDetach(worker->thread);

	std::unordered_map<const void*, ConnectionQueue> discarded;
	{
		std::lock_guard lock(mutex);
		if (calledByWorker) {
			// the calling worker still uses its Worker object and its connection queue: keep both until destruction
			threads = std::move(workers);
			return;
		}
		discarded = std::move(queues);
		queues.clear();
		readyConnections.clear();
		waitingPackets = 0;
	}
	// discarded packets are destroyed here, without holding the lock
}

} // namespace aion::commons::network
