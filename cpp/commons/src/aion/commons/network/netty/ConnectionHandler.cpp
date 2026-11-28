#include "aion/commons/network/netty/ConnectionHandler.h"

#include <chrono>
#include <utility>

#include <asio/bind_executor.hpp>
#include <asio/post.hpp>
#include <fmt/format.h>

#include "aion/commons/network/netty/NettyConnection.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::commons::network::netty {

namespace {

const logging::Logger& log() {
	static const auto* logger = new logging::Logger(logging::LoggerFactory::getLogger("com.aionemu.commons.network.netty.ConnectionHandler"));
	return *logger;
}

/** Java: childOption(ChannelOption.SO_KEEPALIVE, true) */
asio::ip::tcp::socket keepAlive(asio::ip::tcp::socket socket) {
	asio::error_code ignored;
	socket.set_option(asio::socket_base::keep_alive(true), ignored);
	return socket;
}

} // namespace

ConnectionHandler::ConnectionHandler(asio::ip::tcp::socket socket, NioServer& server, int32_t maxPacketSize)
	: AConnectionBase(keepAlive(std::move(socket)), server, maxPacketSize, maxPacketSize), maxPacketSize(maxPacketSize), authTimer(getStrand()),
		stallTimer(getStrand()) {
}

ConnectionHandler::~ConnectionHandler() = default;

void ConnectionHandler::setConnection(std::shared_ptr<NettyConnectionBase> newConnection) {
	connectionRef = newConnection;
	connection = std::move(newConnection);
}

std::string ConnectionHandler::toString() const {
	if (std::shared_ptr<NettyConnectionBase> con = connectionRef.lock())
		return con->toString();
	return "Client " + getIP();
}

bool ConnectionHandler::isWritable() const {
	std::lock_guard lock(guard);
	return writable;
}

int64_t ConnectionHandler::bytesBeforeWritable() const {
	std::lock_guard lock(guard);
	int64_t bytes = pendingWriteBytes - WRITE_BUFFER_LOW_WATER_MARK + 1;
	return bytes <= 0 || writable ? 0 : bytes;
}

void ConnectionHandler::writeAndFlush(const std::function<void(utils::ByteBuffer&)>& writer, bool closeAfterWrite) {
	std::lock_guard lock(guard);
	if (!canSend())
		return; // Netty: the write fails (ClosedChannelException, not logged)
	// PacketEncoder: the packet is written into a buffer of the maximum packet size
	utils::ByteBuffer buffer = utils::ByteBuffer::allocate(maxPacketSize);
	try {
		writer(buffer);
	} catch (...) {
		// Netty: the EncoderException reaches exceptionCaught
		try {
			log().errorCurrentException(fmt::format("Error in connection of {}, disconnecting", toString()));
		} catch (...) {
		}
		closeChannel();
		return;
	}
	std::span<const uint8_t> frame = buffer.remainingSpan();
	sendQueue.emplace_back(frame.begin(), frame.end());
	pendingWriteBytes += static_cast<int64_t>(frame.size());
	updateWritability();
	if (closeAfterWrite)
		beginClose(false); // disconnects once the queue was written
	requestWrite();
}

void ConnectionHandler::closeChannel() {
	std::lock_guard lock(guard);
	for (const std::vector<uint8_t>& packet : sendQueue)
		pendingWriteBytes -= static_cast<int64_t>(packet.size());
	sendQueue.clear();
	beginClose(false);
	requestWrite(); // completes the close as soon as the bytes handed to the socket were written
}

void ConnectionHandler::updateWritability() {
	if (writable && pendingWriteBytes > WRITE_BUFFER_HIGH_WATER_MARK) {
		writable = false;
		channelWritabilityChanged();
	} else if (!writable && pendingWriteBytes < WRITE_BUFFER_LOW_WATER_MARK) {
		writable = true;
		channelWritabilityChanged();
	}
}

void ConnectionHandler::channelWritabilityChanged() {
	int32_t change = ++writabilityChanges;
	std::shared_ptr<NettyConnectionBase> con = connectionRef.lock();
	int64_t stallMillis = con ? con->getMaxSendStallMillis() : 0;
	if (stallMillis > 0 && !writable) {
		asio::post(getStrand(), [self = sharedFromThis(), change, stallMillis] {
			self->stallTimer.expires_after(std::chrono::milliseconds(stallMillis));
			self->stallTimer.async_wait(asio::bind_executor(self->getStrand(), [self, change, stallMillis](const std::error_code& error) {
				if (error)
					return;
				bool stalled;
				{
					std::lock_guard lock(self->guard);
					stalled = change == self->writabilityChanges && self->isActive() && !self->writable;
				}
				if (stalled) {
					log().warn("{} hasn't received data for {} ms while its send buffer is full, disconnecting", self->toString(), stallMillis);
					self->closeChannel();
				}
			}));
		});
	}
}

void ConnectionHandler::initialized() {
	std::shared_ptr<NettyConnectionBase> con = connection;
	con->onConnect();
	int64_t authTimeoutMillis = con->getAuthTimeoutMillis();
	if (authTimeoutMillis > 0) {
		asio::post(getStrand(), [self = sharedFromThis(), authTimeoutMillis] {
			self->authTimer.expires_after(std::chrono::milliseconds(authTimeoutMillis));
			self->authTimer.async_wait(asio::bind_executor(self->getStrand(), [self, authTimeoutMillis](const std::error_code& error) {
				if (error)
					return;
				std::shared_ptr<NettyConnectionBase> con = self->connectionRef.lock();
				if (self->isActive() && con && !con->isAuthenticated()) {
					log().info("{} didn't authenticate within {} ms, disconnecting", self->toString(), authTimeoutMillis);
					self->closeChannel();
				}
			}));
		});
	}
}

bool ConnectionHandler::processData(utils::ByteBuffer& data) {
	if (isPendingClose())
		return true; // Netty closes the channel at once, so nothing received after the close is processed
	try {
		std::unique_ptr<packet::ClientPacketBase> packet = connection->createPacket(data);
		if (!packet)
			return true;
		if (!packet->read())
			return false; // ctx.close()
		if (!connection->execute(std::move(packet))) {
			log().warn("{} has more than {} packets waiting for execution, disconnecting", toString(), connection->getMaxPendingPackets());
			return false;
		}
		return true;
	} catch (...) {
		// Java: exceptionCaught
		try {
			log().errorCurrentException(fmt::format("Error in connection of {}, disconnecting", toString()));
		} catch (...) {
		}
		return false;
	}
}

bool ConnectionHandler::writeData(utils::ByteBuffer& data) {
	// guard is held by the caller
	if (sendQueue.empty())
		return false;
	std::vector<uint8_t> packet = std::move(sendQueue.front());
	sendQueue.pop_front();
	data.put(std::span<const uint8_t>(packet));
	data.flip();
	return true;
}

void ConnectionHandler::onBytesWritten(size_t bytes) noexcept {
	std::lock_guard lock(guard);
	pendingWriteBytes -= static_cast<int64_t>(bytes);
	try {
		updateWritability();
	} catch (...) {
		// posting the stall check failed: nothing to do, the connection is about to break anyway
	}
}

void ConnectionHandler::onSocketClosed() noexcept {
	authTimer.cancel();
	stallTimer.cancel();
}

void ConnectionHandler::onDisconnect() {
	std::shared_ptr<NettyConnectionBase> con = std::move(connection);
	if (con)
		con->executeAfterPendingPackets([con] { con->onDisconnect(); });
}

void ConnectionHandler::onServerClose() {
	closeChannel();
}

void ConnectionHandler::invalidPacket(const std::string& message) const {
	log().warn("{} sent an invalid packet, disconnecting: {}", toString(), message);
}

void ConnectionHandler::onPacketTooLarge(int32_t size, int32_t capacity) const {
	// LengthFieldBasedFrameDecoder (failFast): TooLongFrameException
	invalidPacket(fmt::format("Adjusted frame length exceeds {}: {} - discarded", capacity, size));
}

void ConnectionHandler::onEmptyPacket(int32_t size, const utils::ByteBuffer& buf) const {
	(void)buf;
	if (size < 2) // LengthFieldBasedFrameDecoder: CorruptedFrameException
		invalidPacket(fmt::format("Adjusted frame length ({}) is less than lengthFieldEndOffset: 2", size));
	else
		log().warn("{} sent an empty packet, disconnecting", toString());
}

} // namespace aion::commons::network::netty
