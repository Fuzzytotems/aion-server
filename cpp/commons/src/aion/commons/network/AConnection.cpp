#include "aion/commons/network/AConnection.h"

#include <algorithm>
#include <chrono>
#include <string_view>
#include <typeinfo>

#include <asio/bind_executor.hpp>
#include <asio/post.hpp>
#include <asio/write.hpp>
#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/detail/ServerContext.h"
#include "aion/commons/network/detail/TypeName.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/commons/utils/TimeUtils.h"

namespace aion::commons::network {

namespace {

// the IO logic of AConnection was located in Java's Dispatcher, so its log messages keep that logger name
// Intentionally leaked and created on first use, so the network classes can be constructed and destroyed as statics (Java: static fields) without
// depending on the initialization or destruction order of namespace scope statics.
const logging::Logger& log() {
	static const auto* logger = new logging::Logger(logging::LoggerFactory::getLogger("com.aionemu.commons.network.Dispatcher"));
	return *logger;
}

/** Java: pendingCloseUntilMillis = System.currentTimeMillis() + 2000 in AConnection.close */
constexpr int64_t PENDING_CLOSE_MILLIS = 2000;

std::string remoteIp(const asio::ip::tcp::socket& socket) {
	asio::error_code error;
	asio::ip::tcp::endpoint endpoint = socket.remote_endpoint(error);
	if (error)
		return {};
	asio::ip::address address = endpoint.address();
	if (address.is_v6() && address.to_v6().is_v4_mapped())
		return asio::ip::make_address_v4(asio::ip::v4_mapped, address.to_v6()).to_string();
	return address.to_string();
}

utils::ByteBuffer allocateBuffer(int32_t size, const char* name) {
	if (size < 3)
		throw utils::IllegalArgumentException(fmt::format("{} buffer size must be at least 3 bytes, but was {}", name, size));
	return utils::ByteBuffer::allocate(size);
}

std::string describe(const AConnectionBase& connection) noexcept {
	try {
		return connection.toString();
	} catch (...) {
		return connection.getIP();
	}
}

/** Logs the exception currently being handled as "&lt;message&gt;&lt;connection&gt;"; never throws (for handlers and noexcept paths). */
void logCurrentException(std::string_view message, const AConnectionBase& connection) noexcept {
	try {
		log().errorCurrentException(std::string(message) + describe(connection));
	} catch (...) {
	}
}

} // namespace

AConnectionBase::AConnectionBase(asio::ip::tcp::socket connectedSocket, NioServer& server, int32_t rbSize, int32_t wbSize)
	: context(server.context), socket(std::move(connectedSocket)), strand(asio::make_strand(context->ioContext)), closeTimer(strand),
		ip(remoteIp(socket)), readBuffer(allocateBuffer(rbSize, "Read")), writeBuffer(allocateBuffer(wbSize, "Write")) {
	if (&asio::query(socket.get_executor(), asio::execution::context) != &static_cast<asio::execution_context&>(context->ioContext))
		throw utils::IllegalArgumentException("The socket of a connection must belong to the io_context of its NioServer");
}

AConnectionBase::~AConnectionBase() = default;

std::string AConnectionBase::toString() const {
	return detail::simpleTypeName(typeid(*this)) + " " + ip;
}

void AConnectionBase::close() {
	std::lock_guard lock(guard);
	beginClose(false);
}

bool AConnectionBase::beginClose(bool withClosePacket) {
	if (!canSend())
		return false;

	pendingCloseUntilMillis = utils::currentTimeMillis() + PENDING_CLOSE_MILLIS;
	if (withClosePacket)
		closeWithPacket = true;
	// before startIo (in the constructor, the ConnectionFactory or initialized()), startIo() schedules the close
	if (ioStarted)
		asio::post(strand, [self = shared_from_this()] { self->onCloseScheduled(); });
	return true;
}

void AConnectionBase::requestWrite() {
	if (writeScheduled || !ioStarted)
		return; // before startIo, startIo() starts writing
	asio::post(strand, [self = shared_from_this()] { self->doWrite(); });
	writeScheduled = true;
}

void AConnectionBase::startIo() {
	// guard is held while posting and setting ioStarted, so beginClose and requestWrite either run before (and startIo's handler sees their state)
	// or after (and they post on their own)
	std::lock_guard lock(guard);
	asio::post(strand, [self = shared_from_this()] {
		self->doRead();
		self->doWrite();
		if (self->isPendingClose())
			self->onCloseScheduled();
	});
	ioStarted = true;
}

void AConnectionBase::forceDisconnect() {
	asio::post(strand, [self = shared_from_this()] { self->disconnect(); });
}

void AConnectionBase::doRead() {
	if (closed)
		return;
	auto buffer = asio::buffer(readBuffer.data() + readBuffer.position(), static_cast<size_t>(readBuffer.remaining()));
	socket.async_read_some(buffer, asio::bind_executor(strand, [self = shared_from_this()](const std::error_code& error, size_t bytesRead) {
		self->onRead(error, bytesRead);
	}));
}

void AConnectionBase::onRead(const std::error_code& error, size_t bytesRead) {
	if (closed)
		return;
	if (isClosingWithPacket()) {
		// Deviation: Java removes the read interest in close(closePacket), so neither received data nor EOF or read errors are noticed until the close
		// packet was written. Here reading continues, but data is discarded and EOF or errors just stop reading: the connection is disconnected once
		// the close packet was written (or writing failed, or the close deadline passed). Draining received data avoids that closing a socket with
		// unread data resets the connection, which could discard the close packet on the remote side.
		if (!error && bytesRead > 0) {
			readBuffer.clear();
			doRead();
		}
		return;
	}
	if (error || bytesRead == 0) {
		// read failure, or the remote side shut the socket down cleanly
		disconnect();
		return;
	}

	utils::ByteBuffer& rb = readBuffer;
	rb.position(rb.position() + static_cast<int32_t>(bytesRead));
	rb.flip();
	while (rb.remaining() > 2 && rb.remaining() >= static_cast<uint16_t>(rb.getShort(rb.position()))) {
		// got full message
		if (!parse(rb)) {
			disconnect();
			return;
		}
	}
	if (rb.hasRemaining())
		rb.compact();
	else
		rb.clear();

	if (isClosingWithPacket()) {
		// processData called close(closePacket): the rest of the data will not be processed (Java: the read interest was removed)
		rb.clear();
		doRead();
		return;
	}

	// Deviation: Java waits forever for a packet that is bigger than the read buffer (the selector spins on a full buffer), here the connection is
	// closed as soon as the declared size is known.
	if (rb.position() >= 2) {
		int32_t declaredSize = static_cast<uint16_t>(rb.getShort(0));
		if (declaredSize > rb.capacity()) {
			log().warn(fmt::format("Received packet with a size of {} bytes from {}, which exceeds the read buffer size of {} bytes", declaredSize,
				describe(*this), rb.capacity()));
			disconnect();
			return;
		}
	}

	doRead();
}

bool AConnectionBase::isClosingWithPacket() const {
	std::lock_guard lock(guard);
	return closeWithPacket;
}

bool AConnectionBase::parse(utils::ByteBuffer& buf) {
	int32_t size = static_cast<uint16_t>(buf.getShort()) - 2; // size includes size of the read short, so we need to subtract two bytes
	if (size <= 0) {
		log().warn("Received empty packet without opcode from " + describe(*this) + ", content: " + utils::NetworkUtils::toHex(buf));
		return false;
	}
	utils::ByteBuffer b = buf.slice();
	try {
		b.limit(size);
		// read message fully
		buf.position(buf.position() + size);

		return processData(b);
	} catch (...) {
		try {
			log().errorCurrentException("Error parsing input from " + describe(*this) + ", packet size: " + std::to_string(size) +
																", content: " + utils::NetworkUtils::toHex(b));
		} catch (...) {
		}
		return false;
	}
}

void AConnectionBase::doWrite() {
	bool disconnectNow = false;
	{
		std::lock_guard lock(guard);
		writeScheduled = false;
		if (writeInProgress || closed)
			return; // the completion handler of the write in progress calls doWrite again

		outgoing.clear();
		try {
			// collect several packets into one socket write; each writeData call writes one packet into the cleared write buffer
			while (outgoing.size() < static_cast<size_t>(writeBuffer.capacity())) {
				writeBuffer.clear();
				if (!writeData(writeBuffer))
					break;
				std::span<const uint8_t> bytes = writeBuffer.remainingSpan();
				outgoing.insert(outgoing.end(), bytes.begin(), bytes.end());
			}
		} catch (...) {
			// Deviation: in Java an exception in writeData escapes to the dispatcher loop and leaves a corrupt write buffer behind that gets sent
			try {
				log().errorCurrentException("Error writing data for " + describe(*this));
			} catch (...) {
			}
			disconnectNow = true;
		}
		// Java: a pending close is completed as soon as the send queue is empty (here also all bytes must be handed to the socket)
		if (outgoing.empty() && pendingCloseUntilMillis.load() != 0)
			disconnectNow = true;
	}

	if (disconnectNow) {
		disconnect();
		return;
	}
	if (outgoing.empty())
		return;

	writeInProgress = true;
	asio::async_write(socket, asio::buffer(outgoing), asio::bind_executor(strand, [self = shared_from_this()](const std::error_code& error, size_t) {
		self->writeInProgress = false;
		if (error) {
			self->disconnect();
			return;
		}
		self->doWrite();
	}));
}

void AConnectionBase::onCloseScheduled() {
	if (closed)
		return;
	int64_t remainingMillis = std::max<int64_t>(0, pendingCloseUntilMillis.load() - utils::currentTimeMillis());
	closeTimer.expires_after(std::chrono::milliseconds(remainingMillis));
	closeTimer.async_wait(asio::bind_executor(strand, [self = shared_from_this()](const std::error_code& error) {
		if (!error)
			self->disconnect(); // close deadline passed
	}));
	doWrite();
}

void AConnectionBase::disconnect() {
	{
		std::lock_guard lock(guard);
		if (closed)
			return;
		closed = true;
	}

	asio::error_code ignored;
	socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
	socket.close(ignored);
	closeTimer.cancel();

	if (!registered)
		return;

	dispatchOnDisconnect();
}

void AConnectionBase::dispatchOnDisconnect() noexcept {
	std::shared_ptr<detail::ServerContext> ctx = context;
	std::shared_ptr<AConnectionBase> self;
	try {
		self = shared_from_this();
	} catch (...) {
		return; // unreachable: registered connections are always owned by a shared_ptr
	}
	auto task = [self, ctx] {
		try {
			self->onDisconnect();
		} catch (...) {
			logCurrentException("Error in onDisconnect of ", *self);
		}
		{
			std::lock_guard lock(ctx->mutex);
			ctx->pendingDisconnects--;
		}
		ctx->stateChanged.notify_all();
	};

	{
		// counted before the connection is unregistered, so NioServer::shutdown waits for this callback
		std::lock_guard lock(ctx->mutex);
		ctx->pendingDisconnects++;
	}
	try {
		if (ctx->dcExecutor)
			ctx->dcExecutor(task);
		else
			asio::post(ctx->disconnectContext, task);
	} catch (...) {
		// Deviation: in Java a rejecting dcExecutor throws into the dispatcher and onDisconnect is never called
		logCurrentException("Could not dispatch onDisconnect, calling it on the current thread: ", *self);
		task();
	}
	{
		std::lock_guard lock(ctx->mutex);
		ctx->activeConnections.erase(self);
	}
	ctx->stateChanged.notify_all();
}

} // namespace aion::commons::network
