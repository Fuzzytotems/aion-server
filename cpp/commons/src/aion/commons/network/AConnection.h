#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>

#include "aion/commons/utils/ByteBuffer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network {

class NioServer;

namespace detail {
struct ServerContext;
}

/**
 * Non-template part of AConnection: owns the socket, the strand, the read/write buffers and all IO logic. Concrete connections derive from
 * AConnection&lt;ServerPacket&gt;, which adds the typed send queue.
 * <p>
 * <b>Lifetime.</b> Connections are always held by std::shared_ptr (create them with std::make_shared in the ConnectionFactory). While a
 * connection is registered with a NioServer, the server keeps it alive, and every pending async operation holds a reference as well. After the
 * disconnect, the connection lives as long as other code (e.g. queued client packets) references it.
 * <p>
 * <b>Threading.</b> All socket IO, processData() and writeData() run on the connection's strand, so they never run concurrently for the same
 * connection (one IO thread at a time). sendPacket() and close() may be called from any thread, also before the connection is registered: packets
 * are then sent and the close is processed once IO starts, after initialized() returned. onDisconnect() runs on the server's disconnect executor,
 * never on the strand. The protected recursive mutex {@link #guard} protects the send queue and the close state; writeData() is
 * always invoked with it held, and it may be locked again by subclasses (Java: synchronized (guard)).
 * <p>
 * <b>Reading.</b> Data is framed like Java's Dispatcher: every packet starts with a little endian uint16 length that includes these two bytes.
 * processData() receives one packet body (without the length) at a time, as a slice of the read buffer that is only valid during the call.
 * <p>
 * <b>Closing.</b> close() marks the connection as pending close. It is disconnected as soon as the send queue is empty and all written bytes were
 * handed to the socket, or after 2 seconds at the latest. With a close packet, received data is discarded and EOF or read errors do not disconnect
 * before the close packet was written. Otherwise a read error, EOF, a malformed packet or processData() returning false disconnects immediately.
 * The disconnect happens exactly once: the socket is closed, the connection is unregistered from the server and onDisconnect() is called.
 * <p>
 * Java: com.aionemu.commons.network.AConnection (non-generic part)
 *
 * @author -Nemesiss-
 */
class AConnectionBase : public std::enable_shared_from_this<AConnectionBase> {
public:
	virtual ~AConnectionBase();

	AConnectionBase(const AConnectionBase&) = delete;
	AConnectionBase& operator=(const AConnectionBase&) = delete;

	/**
	 * The connection will be closed at some time (by an IO thread) after pending packets were sent, after that onDisconnect() is called. Does
	 * nothing if the connection is already closing or closed.
	 */
	void close();

	/** @return IP address of the remote side, cached at construction so it is available after the disconnect */
	const std::string& getIP() const noexcept { return ip; }

	/** @return true if this connection is pending close and not closed yet */
	bool isPendingClose() const noexcept { return pendingCloseUntilMillis.load() != 0 && !closed.load(); }

	/** @return true if this connection was disconnected */
	bool isClosed() const noexcept { return closed.load(); }

	/**
	 * @return a shared_ptr to this connection, typed as the object expression it is called on: inside AionConnection, sharedFromThis() is a
	 *         std::shared_ptr&lt;AionConnection&gt; (e.g. for BaseClientPacket::setConnection; Java: this)
	 */
	template <typename Self>
	[[nodiscard]] std::shared_ptr<Self> sharedFromThis(this Self& self) {
		return std::static_pointer_cast<Self>(self.shared_from_this());
	}

	/**
	 * @return a description of this connection for log messages. Subclasses override it like Java's toString() (e.g. "Client 127.0.0.1").
	 * The default is the class name and the IP.
	 */
	virtual std::string toString() const;

protected:
	/**
	 * @param socket
	 *          the connected socket; it must belong to the server's io_context (sockets passed to a ConnectionFactory do)
	 * @param server
	 *          the server this connection will be registered with
	 * @param rbSize
	 *          read buffer size, the maximum size of a received packet (including its length prefix)
	 * @param wbSize
	 *          write buffer size, the maximum size of a single sent packet
	 */
	AConnectionBase(asio::ip::tcp::socket socket, NioServer& server, int32_t rbSize, int32_t wbSize);

	/**
	 * Called for every received packet, on the connection's strand. data contains one packet body: [position, limit) excluding the length
	 * prefix. The buffer shares the connection's read buffer, so it must not be used after this method returns.
	 * Exceptions are logged (with a hex dump) and close the connection.
	 *
	 * @return true if data was processed correctly, false if some error occurred and the connection should be closed NOW
	 */
	virtual bool processData(utils::ByteBuffer& data) = 0;

	/**
	 * Called on the connection's strand with {@link #guard} held, repeatedly until it returns false. data is a cleared buffer (position 0, limit
	 * = capacity) of the write buffer size. Implementations take the next packet from the send queue and write it into data, so that
	 * [position, limit) are the bytes to send (e.g. by flipping the buffer).
	 * Exceptions (e.g. a packet exceeding the buffer) are logged and disconnect the connection.
	 *
	 * @return true if data was written to the buffer, false if there is no more data to write
	 */
	virtual bool writeData(utils::ByteBuffer& data) = 0;

	/**
	 * Called once the connection is registered with the server and ready to send packets, before the first read is started (so before any
	 * processData call). May be used as a hook for sending the first packet. Runs on the accepting IO thread, or on the thread calling
	 * NioServer::registerConnection for outbound connections. A close() or close(closePacket) called before or during initialized() (e.g. in the
	 * ConnectionFactory) takes effect after it returned.
	 */
	virtual void initialized() = 0;

	/** Called exactly once after this connection was disconnected, on a disconnect executor thread. */
	virtual void onDisconnect() = 0;

	/** Called by NioServer::shutdown to inform the connection that the server is shutting down (it should close itself). Called only once. */
	virtual void onServerClose() = 0;

	/**
	 * Marks the connection as pending close and schedules the close on its strand (or lets startIo do it). The caller must hold {@link #guard}.
	 * @param withClosePacket
	 *          true if a close packet will be queued by the caller: received data is not processed anymore (Java: interest ops = OP_WRITE)
	 * @return false if the connection is already closing or closed (nothing was done)
	 */
	bool beginClose(bool withClosePacket);

	/** @return false if packets must be ignored because the connection is closing or closed. The caller must hold {@link #guard}. */
	bool canSend() const noexcept { return pendingCloseUntilMillis.load() == 0 && !closed.load(); }

	/** Schedules the write loop on the strand if necessary. The caller must hold {@link #guard}. */
	void requestWrite();

	/** Lock for the send queue and the close state (Java: guard). Recursive, since writeData implementations may call sendPacket or close. */
	mutable std::recursive_mutex guard;

private:
	friend class NioServer;

	/** Starts reading and writing, and processes a close requested before, called by NioServer after initialized(). */
	void startIo();
	/** Disconnects on the strand (bypassing the close deadline), used by NioServer::shutdown for connections that did not close. */
	void forceDisconnect();

	void doRead();
	void onRead(const std::error_code& error, size_t bytesRead);
	bool isClosingWithPacket() const;
	bool parse(utils::ByteBuffer& buf);
	void doWrite();
	void onCloseScheduled();
	/** Closes the socket and calls onDisconnect() on the disconnect executor, exactly once. Must run on the strand. */
	void disconnect();
	/** Runs onDisconnect() on the disconnect executor and unregisters the connection from the server. */
	void dispatchOnDisconnect() noexcept;

	std::shared_ptr<detail::ServerContext> context;
	asio::ip::tcp::socket socket;
	asio::strand<asio::io_context::executor_type> strand;
	asio::steady_timer closeTimer;
	const std::string ip;

	utils::ByteBuffer readBuffer; // strand only
	utils::ByteBuffer writeBuffer; // strand only (with guard held while writeData fills it)
	std::vector<uint8_t> outgoing; // strand only: bytes of the async_write in progress

	/** Wall clock time (ms) after which a closing connection is force-closed, 0 if not closing (Java: pendingCloseUntilMillis) */
	std::atomic<int64_t> pendingCloseUntilMillis = 0;
	/** true once disconnected (Java: closed); set with guard held */
	std::atomic<bool> closed = false;
	/** true if close was called with a close packet, which stops reading (guarded by guard) */
	bool closeWithPacket = false;
	/** true once startIo posted its handler: from then on, beginClose and requestWrite post to the strand themselves (guarded by guard) */
	bool ioStarted = false;
	/** true if a doWrite is posted to the strand and has not started yet (guarded by guard) */
	bool writeScheduled = false;
	/** true while an async_write is in progress (strand only) */
	bool writeInProgress = false;
	/** true once registered with the server (set by NioServer before initialized()) */
	std::atomic<bool> registered = false;
};

/** Makes connections formattable with fmt ("{}"), using toString(). */
inline std::string format_as(const AConnectionBase& connection) {
	return connection.toString();
}

/**
 * Base class of all connections, with a send queue of server packets of type TServerPacket.
 * <p>
 * Subclasses implement the hooks of AConnectionBase. A typical writeData implementation:
 * <pre>
 * bool writeData(utils::ByteBuffer&amp; data) override { // guard is held
 * 	if (sendMsgQueue.empty())
 * 		return false;
 * 	auto packet = std::move(sendMsgQueue.front());
 * 	sendMsgQueue.pop_front();
 * 	packet-&gt;write(*this, data); // writes the packet and flips the buffer
 * 	return true;
 * }
 * </pre>
 * Java: com.aionemu.commons.network.AConnection
 *
 * @author -Nemesiss-
 */
template <typename TServerPacket>
class AConnection : public AConnectionBase {
public:
	using ServerPacket = TServerPacket;

	/**
	 * Queues the packet to be sent to the remote side. The packet is ignored if the connection is closing or closed (or if it is null).
	 * The same packet instance may be sent to several connections.
	 */
	void sendPacket(std::shared_ptr<TServerPacket> serverPacket) {
		if (!serverPacket)
			return;
		std::lock_guard lock(guard);
		if (!canSend())
			return;
		sendMsgQueue.push_back(std::move(serverPacket));
		requestWrite();
	}

	using AConnectionBase::close;

	/**
	 * It is guaranteed that closePacket will be sent before closing the connection, but all previously queued and future packets won't.
	 * The connection will be closed (by an IO thread) and onDisconnect() will be called.
	 *
	 * @param closePacket
	 *          packet that will be sent before closing. If it is null, the regular close() is performed instead.
	 */
	void close(std::shared_ptr<TServerPacket> closePacket) {
		if (!closePacket) {
			close();
			return;
		}
		std::lock_guard lock(guard);
		if (!beginClose(true))
			return;
		sendMsgQueue.clear();
		sendMsgQueue.push_back(std::move(closePacket));
	}

protected:
	AConnection(asio::ip::tcp::socket socket, NioServer& server, int32_t rbSize, int32_t wbSize)
		: AConnectionBase(std::move(socket), server, rbSize, wbSize) {}

	/** Java: getSendMsgQueue(). The caller must hold guard. */
	std::deque<std::shared_ptr<TServerPacket>>& getSendMsgQueue() noexcept { return sendMsgQueue; }

	/** Server packets "to send" queue, guarded by guard. */
	std::deque<std::shared_ptr<TServerPacket>> sendMsgQueue;
};

} // namespace aion::commons::network
