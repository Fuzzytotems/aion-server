#include "aion/chatserver/network/NetConnector.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/chatserver/network/aion/AionConnection.h"
#include "aion/chatserver/network/gameserver/GsConnection.h"
#include "aion/chatserver/network/netty/ConnectionHandler.h"
#include "aion/chatserver/network/netty/NettyPipeline.h"
#include "aion/chatserver/network/netty/PacketExecutorGroup.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::network::NetConnector {

using commons::network::NioServer;
using commons::network::ServerCfg;
using configs::network::NetworkConfig;
using netty::ConnectionHandler;
using netty::NettyConnectionBase;

namespace {

/** Java: shutdownGracefully(quietPeriodMillis, timeoutMillis) of the packet executors */
constexpr std::chrono::milliseconds PACKET_EXECUTORS_SHUTDOWN_TIMEOUT{3000};

using ConnectionFactory = std::function<std::shared_ptr<NettyConnectionBase>(std::shared_ptr<ConnectionHandler> channel,
	NettyConnectionBase::Executor packetExecutor)>;

struct State {
	std::mutex mutex;
	std::shared_ptr<netty::PacketExecutorGroup> packetExecutors;
	std::unique_ptr<NioServer> server;
};

// leaked: connections referencing the executors may outlive static destruction
State& state() {
	static auto* s = new State();
	return *s;
}

/** Java: bind(address, clientDescription, maxPacketSize, connectionFactory), returning the listener config */
ServerCfg bindCfg(const commons::utils::InetSocketAddress& address, std::string clientDescription, int32_t maxPacketSize,
	std::shared_ptr<netty::PacketExecutorGroup> packetExecutors, ConnectionFactory connectionFactory) {
	return ServerCfg{address, std::move(clientDescription),
		[=](asio::ip::tcp::socket socket, NioServer& server) -> std::shared_ptr<commons::network::AConnectionBase> {
			auto channel = std::make_shared<ConnectionHandler>(std::move(socket), server, maxPacketSize);
			netty::NettyPipeline::init(*channel, connectionFactory(channel, packetExecutors->next()));
			return channel;
		}};
}

} // namespace

void connect() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	if (s.server)
		throw commons::utils::IllegalStateException("NetConnector is already connected");

	auto packetExecutors = std::make_shared<netty::PacketExecutorGroup>(8);
	std::vector<ServerCfg> cfgs;
	cfgs.push_back(bindCfg(NetworkConfig::CLIENT_SOCKET_ADDRESS, "Aion game clients", 8192 * 2, packetExecutors,
		[](std::shared_ptr<ConnectionHandler> channel, NettyConnectionBase::Executor executor) {
			return std::make_shared<aion::AionConnection>(std::move(channel), std::move(executor));
		}));
	cfgs.push_back(bindCfg(NetworkConfig::GAMESERVER_SOCKET_ADDRESS, "game servers", 8192 * 8, packetExecutors,
		[](std::shared_ptr<ConnectionHandler> channel, NettyConnectionBase::Executor executor) {
			return std::make_shared<gameserver::GsConnection>(std::move(channel), std::move(executor));
		}));
	try {
		auto server = std::make_unique<NioServer>(std::max(1, NetworkConfig::NIO_READ_WRITE_THREADS), std::move(cfgs), 1);
		server->connect(); // logs "Listening on <address info> for <client description>" for both
		s.server = std::move(server);
		s.packetExecutors = std::move(packetExecutors);
	} catch (...) {
		packetExecutors->shutdown(std::chrono::milliseconds(0));
		throw;
	}
}

void shutdown() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	if (!s.server)
		return;
	s.server->shutdown(); // Java: channels.close()
	s.packetExecutors->shutdown(PACKET_EXECUTORS_SHUTDOWN_TIMEOUT);
	s.server.reset();
	s.packetExecutors.reset();
}

std::vector<commons::utils::InetSocketAddress> getBoundAddresses() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	return s.server ? s.server->getBoundAddresses() : std::vector<commons::utils::InetSocketAddress>{};
}

std::shared_ptr<netty::PacketExecutorGroup> getPacketExecutors() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	return s.packetExecutors;
}

} // namespace aion::chatserver::network::NetConnector
