#include "aion/chatserver/network/netty/handler/AbstractChannelHandler.h"

#include <memory>
#include <typeinfo>
#include <utility>

#include "aion/chatserver/common/netty/BaseServerPacket.h"
#include "aion/chatserver/network/netty/coder/LoginPacketEncoder.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/ClassName.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::network::netty::handler {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.netty.handler.AbstractChannelHandler"));
	return *logger;
}

/** Java: java.lang.ClassCastException, see close(packet) */
class ClassCastException : public commons::utils::Exception {
public:
	using Exception::Exception;
};

} // namespace

AbstractChannelHandler::AbstractChannelHandler(asio::ip::tcp::socket socket, commons::network::NioServer& server, int32_t rbSize, int32_t wbSize)
	: AConnection(std::move(socket), server, rbSize, wbSize) {
}

void AbstractChannelHandler::channelDisconnected() {
	log().info("Channel disconnected IP: {}", getIP());
}

void AbstractChannelHandler::exceptionCaught(std::exception_ptr cause) const {
	try {
		std::rethrow_exception(cause);
	} catch (const commons::utils::IOException&) {
		return;
	} catch (...) {
		try {
			log().errorCurrentException("Caught exception from netty");
		} catch (...) {
		}
	}
}

void AbstractChannelHandler::close(const common::netty::BaseServerPacket& packet) {
	exceptionCaught(std::make_exception_ptr(ClassCastException("class " + commons::utils::getSimpleClassName(typeid(packet)) +
																															" cannot be cast to class org.jboss.netty.buffer.ChannelBuffer")));
	close();
}

void AbstractChannelHandler::write(common::netty::ChannelBuffer cb) {
	coder::LoginPacketEncoder::encode(cb);
	// Deviation: only the encoded bytes are queued; Netty queues the 16 KiB buffer the packet was written into until the socket took it
	auto packet = std::make_shared<common::netty::ChannelBuffer>(common::netty::ChannelBuffer::allocate(cb.remaining()));
	packet->put(cb);
	packet->flip();
	AConnection::sendPacket(std::move(packet));
}

bool AbstractChannelHandler::writeData(commons::utils::ByteBuffer& data) {
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<common::netty::ChannelBuffer> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();
	common::netty::ChannelBuffer bytes = *packet; // own position, shared storage
	data.put(bytes);
	data.flip();
	return true;
}

} // namespace aion::chatserver::network::netty::handler
