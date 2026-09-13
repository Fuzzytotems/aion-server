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
#include <utility>
#include <vector>

#include "aion/commons/network/packet/BaseClientPacket.h"

namespace aion::commons::network {

/**
 * Non-template part of PacketProcessor: the worker threads, the per connection queues and the thread count checker. Use
 * PacketProcessor&lt;Connection&gt;.
 */
class PacketProcessorBase {
public:
	/**
	 * Decorator that executes a packet (Java: the Executor passed to PacketProcessor, e.g. ExecuteWrapper, which measures the execution time).
	 * It must call packet.run(). Exceptions escaping it are logged by the processor.
	 */
	using Executor = std::function<void(packet::ClientPacketBase& packet)>;

	/** Java: CheckerTask.CHECK_INTERVAL */
	static constexpr std::chrono::minutes CHECK_INTERVAL{1};

	virtual ~PacketProcessorBase();

	PacketProcessorBase(const PacketProcessorBase&) = delete;
	PacketProcessorBase& operator=(const PacketProcessorBase&) = delete;

	/**
	 * Stops all threads: packets currently running are finished, packets still waiting are discarded, and packets passed to executePacket
	 * afterwards are discarded as well. Blocks until the worker threads (except the calling one, if called from a packet) have exited. Calling it
	 * again does nothing. The destructor calls it; destroying the processor from one of its own packets is not supported.
	 */
	void shutdown();

	/**
	 * Performs one run of the thread count checker, which normally runs every CHECK_INTERVAL on the "PacketProcessor:Checker" thread (only if
	 * minThreads != maxThreads): if the number of waiting packets did not grow since the last check and is at most threadKillThreshold, one
	 * worker thread is stopped (if more than minThreads run); otherwise, if it exceeds threadSpawnThreshold, a worker thread is added (if fewer
	 * than maxThreads run) or "Lag detected!" is logged when no thread can be added and at least 3 * threadSpawnThreshold packets wait.
	 * Public so the rules can be tested deterministically.
	 */
	void checkThreadCount();

	/** @return current number of worker threads */
	int32_t getThreadCount() const;

	/** @return number of packets waiting for execution (not including running ones) */
	int32_t getWaitingPacketCount() const;

protected:
	PacketProcessorBase(int32_t minThreads, int32_t maxThreads, int32_t threadSpawnThreshold, int32_t threadKillThreshold, Executor executor);

	/** Queues the packet; packets with the same connectionKey are executed one at a time in queue order. */
	void execute(const void* connectionKey, std::unique_ptr<packet::ClientPacketBase> packet);

private:
	struct Worker {
		std::string name;
		std::thread thread;
		bool killed = false; // guarded by mutex
		bool finished = false; // guarded by mutex
	};

	/** The packets of one connection. Exists while the connection has waiting or running packets. */
	struct ConnectionQueue {
		std::deque<std::unique_ptr<packet::ClientPacketBase>> packets;
		/** true while a worker executes a packet of this connection (Java: AConnection.tryLockConnection) */
		bool running = false;
	};

	bool newThread();
	void killThread();
	void joinFinishedThreads();
	void workerLoop(Worker& worker);
	void checkerLoop();
	void runPacket(packet::ClientPacketBase& packet) const;

	const int32_t minThreads;
	const int32_t maxThreads;
	const int32_t threadSpawnThreshold;
	const int32_t threadKillThreshold;
	const Executor executor;

	mutable std::mutex mutex;
	std::condition_variable notEmpty;
	/** notified on shutdown to wake the checker */
	std::condition_variable shutdownRequested;
	/** notified when a worker thread exits */
	std::condition_variable workerFinished;
	std::unordered_map<const void*, ConnectionQueue> queues;
	/** connections with waiting packets and no running packet, in the order they became ready */
	std::deque<const void*> readyConnections;
	int32_t waitingPackets = 0;
	int32_t previousPacketCount = 0; // checker state
	bool shuttingDown = false;
	std::vector<std::unique_ptr<Worker>> threads;
	/** killed workers that were not joined yet */
	std::vector<std::unique_ptr<Worker>> killedThreads;
	std::thread checkerThread;
};

/**
 * Packet processor responsible for executing client packets on a pool of worker threads, respecting these rules:
 * <ul>
 * <li>only one packet per connection is executed at a time</li>
 * <li>packets of a connection are executed in the order they were received</li>
 * </ul>
 * Packets of different connections run in parallel on up to maxThreads threads (named "PacketProcessor:&lt;n&gt;"). The thread count is adjusted
 * between minThreads and maxThreads by a checker thread (see checkThreadCount()). Exceptions thrown while executing a packet are logged and never
 * stop a worker.
 * <p>
 * Packets are passed as std::unique_ptr: after reading, a packet has exactly one owner (the queue, then the executing worker) and is destroyed
 * right after its execution. Shared state lives in the connection, which the packet keeps alive through its shared_ptr.
 * <p>
 * Deviation: Java scans one global packet list for the first packet whose connection is not busy. Here each connection has its own queue and
 * ready connections are served in FIFO order, which gives the same guarantees without a linear scan.
 * <p>
 * Java: com.aionemu.commons.network.PacketProcessor
 *
 * @author -Nemesiss-
 */
template <typename TConnection>
class PacketProcessor : public PacketProcessorBase {
public:
	/**
	 * Creates and starts the packet processor.
	 *
	 * @param minThreads
	 *          minimum number of worker threads (&gt; 0)
	 * @param maxThreads
	 *          maximum number of worker threads (&gt;= minThreads)
	 * @param threadSpawnThreshold
	 *          if more packets than this wait for execution, a new thread is spawned (&gt; 0)
	 * @param threadKillThreshold
	 *          if at most this many packets wait for execution, a thread is stopped (&gt; 0)
	 * @param executor
	 *          optional decorator executing the packets, see Executor
	 * @throws utils::IllegalArgumentException
	 *           if a parameter is invalid (same messages as Java)
	 */
	PacketProcessor(int32_t minThreads, int32_t maxThreads, int32_t threadSpawnThreshold, int32_t threadKillThreshold, Executor executor = {})
		: PacketProcessorBase(minThreads, maxThreads, threadSpawnThreshold, threadKillThreshold, std::move(executor)) {}

	/** Adds the packet to the execution queue; it will be executed as soon as possible on a worker thread. */
	void executePacket(std::unique_ptr<packet::BaseClientPacket<TConnection>> packet) {
		if (!packet)
			return;
		const void* connectionKey = packet->getConnection().get();
		execute(connectionKey, std::move(packet));
	}
};

} // namespace aion::commons::network
