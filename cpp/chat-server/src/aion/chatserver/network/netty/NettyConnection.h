#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/commons/utils/ByteBuffer.h"

namespace aion::chatserver::network::netty {

class ConnectionHandler;

/**
 * Non-template part of NettyConnection: the channel, the packet executor and everything that does not depend on the server packet type. Use
 * NettyConnection&lt;ServerPacket&gt; as the base class of connections.
 */
class NettyConnectionBase : public std::enable_shared_from_this<NettyConnectionBase> {
public:
	/**
	 * Executes tasks (Java: java.util.concurrent.Executor). The executor of a connection must run its tasks one after another in submission order
	 * (Java: a Netty EventExecutor), see NetConnector.
	 */
	using Executor = std::function<void(std::function<void()> task)>;

	virtual ~NettyConnectionBase();

	NettyConnectionBase(const NettyConnectionBase&) = delete;
	NettyConnectionBase& operator=(const NettyConnectionBase&) = delete;

	/** Closes the connection at once (Java: channel.close()): packets that were not handed to the socket yet are dropped. */
	void close();

	/** @return the IP address of the remote side */
	const std::string& getIP() const noexcept { return ip; }

	/** @return true if the remote side connected from a loopback address */
	bool isFromLoopback() const noexcept { return fromLoopback; }

	/** @return a description of this connection for log messages (Java: toString(), overridden by every connection) */
	virtual std::string toString() const = 0;

protected:
	NettyConnectionBase(std::shared_ptr<ConnectionHandler> channel, Executor packetExecutor);

	/**
	 * Java: sendPacket(S) - unless more than getMaxPendingWriteBytes() bytes wait to be sent (which closes the connection), the writer writes the
	 * packet, including its size header, into a buffer of the maximum packet size on the calling thread and the bytes are queued.
	 */
	void send(const std::function<void(commons::utils::ByteBuffer&)>& writer);

	/** Java: close(S) - like send, but the connection is closed once the packet was written. */
	void closeAfter(const std::function<void(commons::utils::ByteBuffer&)>& writer);

	/**
	 * @return The packet for the given data, or null if it should be ignored. data is the packet body after the size header, valid during the
	 *         call only (the packet reads it in read()).
	 */
	virtual std::unique_ptr<commons::network::packet::ClientPacketBase> createPacket(commons::utils::ByteBuffer& data) = 0;

	/**
	 * @return True if the remote side has authenticated itself. Connections that didn't authenticate within getAuthTimeoutMillis() get closed.
	 */
	virtual bool isAuthenticated() const { return true; }

	/** @return The time in milliseconds the remote side has to authenticate itself, or 0 for no limit. */
	virtual int64_t getAuthTimeoutMillis() const { return 0; }

	/** @return The maximum number of received packets that may wait for execution before the connection gets closed. */
	virtual int32_t getMaxPendingPackets() const = 0;

	/** @return The maximum number of bytes that may wait to be sent before the connection gets closed. */
	virtual int64_t getMaxPendingWriteBytes() const { return std::numeric_limits<int64_t>::max(); }

	/**
	 * @return The maximum time in milliseconds the remote side may not receive data while the send buffer is full, before the connection gets
	 *         closed.
	 */
	virtual int64_t getMaxSendStallMillis() const { return 0; }

	virtual void onConnect() = 0;

	/** Called once after the connection was closed and all of its pending packets were executed. */
	virtual void onDisconnect() = 0;

private:
	friend class ConnectionHandler;

	/**
	 * Queues the packet for execution after all previously received packets of this connection.
	 *
	 * @return False if the maximum number of packets is already waiting for execution.
	 */
	bool execute(std::unique_ptr<commons::network::packet::ClientPacketBase> packet);

	/** Runs the task after all packets of this connection, which are already queued for execution. */
	void executeAfterPendingPackets(std::function<void()> task);

	const std::shared_ptr<ConnectionHandler> channel;
	const Executor packetExecutor;
	const std::string ip;
	const bool fromLoopback;
	std::atomic<int32_t> pendingPackets = 0;
};

/**
 * Connection on top of a channel (ConnectionHandler). Received packets are executed one after another on the connection's packet executor,
 * outside the I/O threads.
 * <p>
 * C++: Netty's channel is the ConnectionHandler, a commons connection on the asio IO threads (see there for how the pipeline maps onto it).
 * Packets are written into their buffer on the thread that sends them (Netty: on the channel's event loop), in the order they are sent.
 * <p>
 * Java: com.aionemu.chatserver.network.netty.NettyConnection
 *
 * @param S the server packet type
 */
template <typename S>
class NettyConnection : public NettyConnectionBase {
public:
	/** Sends the packet (see NettyConnectionBase::send). */
	void sendPacket(const S& packet) {
		send([this, &packet](commons::utils::ByteBuffer& buffer) { writePacket(packet, buffer); });
	}

	using NettyConnectionBase::close;

	/** Sends the packet and closes the connection afterwards. */
	void close(const S& packet) {
		closeAfter([this, &packet](commons::utils::ByteBuffer& buffer) { writePacket(packet, buffer); });
	}

protected:
	NettyConnection(std::shared_ptr<ConnectionHandler> channel, Executor packetExecutor)
		: NettyConnectionBase(std::move(channel), std::move(packetExecutor)) {}

	/** Writes the packet including its size header into the buffer, so that [position, limit) is the frame afterwards. */
	virtual void writePacket(const S& packet, commons::utils::ByteBuffer& buffer) = 0;
};

} // namespace aion::chatserver::network::netty
