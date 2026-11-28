#include "aion/chatserver/network/netty/NettyConnection.h"

#include <utility>

#include <asio/ip/address.hpp>

#include "aion/chatserver/network/netty/ConnectionHandler.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::netty {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.netty.NettyConnection"));
	return *logger;
}

bool isLoopback(const std::string& ip) {
	asio::error_code error;
	asio::ip::address address = asio::ip::make_address(ip, error);
	return !error && address.is_loopback();
}

} // namespace

NettyConnectionBase::NettyConnectionBase(std::shared_ptr<ConnectionHandler> channel, Executor packetExecutor)
	: channel(std::move(channel)), packetExecutor(std::move(packetExecutor)), ip(this->channel->getIP()), fromLoopback(isLoopback(ip)) {
}

NettyConnectionBase::~NettyConnectionBase() = default;

void NettyConnectionBase::send(const std::function<void(commons::utils::ByteBuffer&)>& writer) {
	if (!channel->isWritable() && channel->bytesBeforeWritable() > getMaxPendingWriteBytes()) {
		log().warn("{} has more than {} bytes waiting to be sent, disconnecting", toString(), getMaxPendingWriteBytes());
		close();
		return;
	}
	channel->writeAndFlush(writer, false);
}

void NettyConnectionBase::close() {
	channel->closeChannel();
}

void NettyConnectionBase::closeAfter(const std::function<void(commons::utils::ByteBuffer&)>& writer) {
	channel->writeAndFlush(writer, true);
}

bool NettyConnectionBase::execute(std::unique_ptr<commons::network::packet::ClientPacketBase> packet) {
	if (pendingPackets.fetch_add(1) + 1 > getMaxPendingPackets()) {
		pendingPackets--;
		return false;
	}
	std::shared_ptr<commons::network::packet::ClientPacketBase> task(std::move(packet));
	packetExecutor([self = shared_from_this(), task] {
		struct Finally {
			NettyConnectionBase& connection;
			~Finally() { connection.pendingPackets--; }
		} decrement{*self};
		task->run();
	});
	return true;
}

void NettyConnectionBase::executeAfterPendingPackets(std::function<void()> task) {
	packetExecutor(std::move(task));
}

} // namespace aion::chatserver::network::netty
