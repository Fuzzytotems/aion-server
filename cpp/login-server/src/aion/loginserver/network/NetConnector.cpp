#include "aion/loginserver/network/NetConnector.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <utility>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/netty/ConnectionHandler.h"
#include "aion/commons/network/netty/NettyPipeline.h"
#include "aion/commons/network/netty/PacketExecutorGroup.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/network/aion/LoginConnection.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/utils/ScheduledExecutor.h"

namespace aion::loginserver::network::NetConnector {

using commons::network::NioServer;
using commons::network::ServerCfg;
using commons::network::netty::ConnectionHandler;
using commons::network::netty::NettyConnectionBase;
using commons::network::netty::PacketExecutorGroup;
using configs::Config;
using gameserver::GsConnection;
using ::aion::loginserver::network::aion::LoginConnection;

namespace {

/** Java: shutdownGracefully(quietPeriodMillis, timeoutMillis) of the client packet executors */
constexpr std::chrono::milliseconds CLIENT_PACKET_EXECUTORS_SHUTDOWN_TIMEOUT{3000};
/** Java: awaitTermination(1, TimeUnit.SECONDS) in GsConnection.shutdownExecutors */
constexpr std::chrono::seconds GS_EXECUTORS_SHUTDOWN_TIMEOUT{1};

using ConnectionFactory = std::function<std::shared_ptr<NettyConnectionBase>(std::shared_ptr<ConnectionHandler> channel)>;

struct State {
	std::mutex mutex;
	std::shared_ptr<PacketExecutorGroup> clientPacketExecutors;
	std::shared_ptr<PacketExecutorGroup> gsPacketExecutors;
	std::shared_ptr<utils::ScheduledExecutor> pingPongExecutor;
	std::unique_ptr<NioServer> instance;
};

// leaked: connections referencing the executors may outlive static destruction
State& state() {
	static auto* s = new State();
	return *s;
}

/** Java: GsConnection.shutdownExecutors, after the client packet executors */
void stopExecutors(State& s) {
	if (s.clientPacketExecutors)
		s.clientPacketExecutors->shutdown(CLIENT_PACKET_EXECUTORS_SHUTDOWN_TIMEOUT);
	if (s.pingPongExecutor)
		s.pingPongExecutor->shutdown();
	if (s.gsPacketExecutors)
		s.gsPacketExecutors->shutdown(GS_EXECUTORS_SHUTDOWN_TIMEOUT);
	if (s.pingPongExecutor)
		s.pingPongExecutor->awaitTermination(GS_EXECUTORS_SHUTDOWN_TIMEOUT);
	s.clientPacketExecutors.reset();
	s.gsPacketExecutors.reset();
	s.pingPongExecutor.reset();
}

/** Java: bind(address, clientDescription, maxPacketSize, connectionFactory), returning the listener config */
ServerCfg bindCfg(const commons::utils::InetSocketAddress& address, std::string clientDescription, int32_t maxPacketSize,
	ConnectionFactory connectionFactory) {
	return ServerCfg{address, std::move(clientDescription),
		[=](asio::ip::tcp::socket socket, NioServer& server) -> std::shared_ptr<commons::network::AConnectionBase> {
			auto channel = std::make_shared<ConnectionHandler>(std::move(socket), server, maxPacketSize);
			commons::network::netty::NettyPipeline::init(*channel, connectionFactory(channel));
			return channel;
		}};
}

} // namespace

void connect() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	if (s.instance)
		throw commons::utils::IllegalStateException("NetConnector is already connected");

	auto clientPacketExecutors = std::make_shared<PacketExecutorGroup>(8);
	// Deviation: Java runs game server packets on a cached thread pool (unordered, unbounded); see GsConnection
	auto gsPacketExecutors = std::make_shared<PacketExecutorGroup>(8);
	auto pingPongExecutor = std::make_shared<utils::ScheduledExecutor>("PingPong");

	std::vector<ServerCfg> cfgs;
	cfgs.push_back(bindCfg(Config::CLIENT_SOCKET_ADDRESS, "Aion game clients", 8192 * 2, [clientPacketExecutors](std::shared_ptr<ConnectionHandler> channel) {
		return std::make_shared<LoginConnection>(std::move(channel), clientPacketExecutors->next());
	}));
	cfgs.push_back(bindCfg(Config::GAMESERVER_SOCKET_ADDRESS, "game servers", 8192 * 8,
		[gsPacketExecutors, pingPongExecutor](std::shared_ptr<ConnectionHandler> channel) {
			return std::make_shared<GsConnection>(std::move(channel), gsPacketExecutors->next(), pingPongExecutor);
		}));

	s.clientPacketExecutors = clientPacketExecutors;
	s.gsPacketExecutors = gsPacketExecutors;
	s.pingPongExecutor = pingPongExecutor;
	try {
		auto instance = std::make_unique<NioServer>(std::max(1, Config::NIO_READ_WRITE_THREADS), std::move(cfgs), DISCONNECT_THREADS);
		instance->connect();
		s.instance = std::move(instance);
	} catch (...) {
		stopExecutors(s);
		throw;
	}
}

void shutdown() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	if (!s.instance)
		return;
	s.instance->shutdown(); // Java: channels.close()
	stopExecutors(s);
	s.instance.reset();
}

std::vector<commons::utils::InetSocketAddress> getBoundAddresses() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	return s.instance ? s.instance->getBoundAddresses() : std::vector<commons::utils::InetSocketAddress>{};
}

} // namespace aion::loginserver::network::NetConnector
