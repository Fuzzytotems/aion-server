#include "aion/loginserver/network/NetConnector.h"

#include <memory>
#include <mutex>
#include <utility>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/network/aion/LoginConnection.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/utils/ScheduledExecutor.h"

namespace aion::loginserver::network::NetConnector {

using commons::network::NioServer;
using commons::network::ServerCfg;
using configs::Config;
using gameserver::GsConnection;
using ::aion::loginserver::network::aion::LoginConnection;

namespace {

struct State {
	std::mutex mutex;
	std::shared_ptr<LoginConnection::Processor> loginProcessor;
	std::shared_ptr<GsConnection::Processor> gsProcessor;
	std::shared_ptr<utils::ScheduledExecutor> pingPongExecutor;
	std::unique_ptr<NioServer> instance;
};

// leaked: connections referencing the executors may outlive static destruction
State& state() {
	static auto* s = new State();
	return *s;
}

void stopExecutors(State& s) {
	if (s.loginProcessor)
		s.loginProcessor->shutdown();
	if (s.gsProcessor)
		s.gsProcessor->shutdown();
	if (s.pingPongExecutor) {
		s.pingPongExecutor->shutdown();
		s.pingPongExecutor->awaitTermination(std::chrono::seconds(5));
	}
	s.loginProcessor.reset();
	s.gsProcessor.reset();
	s.pingPongExecutor.reset();
}

} // namespace

void connect() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	if (s.instance)
		throw commons::utils::IllegalStateException("NetConnector is already connected");

	auto loginProcessor = std::make_shared<LoginConnection::Processor>(1, 8, 50, 3);
	// Deviation: Java runs game server packets on a cached thread pool (unordered, unbounded); see GsConnection
	auto gsProcessor = std::make_shared<GsConnection::Processor>(4, 8, 50, 3);
	auto pingPongExecutor = std::make_shared<utils::ScheduledExecutor>("PingPong");

	ServerCfg aionCfg{Config::CLIENT_SOCKET_ADDRESS, "Aion game clients", [loginProcessor](asio::ip::tcp::socket socket, NioServer& server) {
		return std::make_shared<LoginConnection>(std::move(socket), server, loginProcessor);
	}};
	ServerCfg gsCfg{Config::GAMESERVER_SOCKET_ADDRESS, "game servers", [gsProcessor, pingPongExecutor](asio::ip::tcp::socket socket, NioServer& server) {
		return std::make_shared<GsConnection>(std::move(socket), server, gsProcessor, pingPongExecutor);
	}};

	s.loginProcessor = loginProcessor;
	s.gsProcessor = gsProcessor;
	s.pingPongExecutor = pingPongExecutor;
	try {
		auto instance = std::make_unique<NioServer>(Config::NIO_READ_WRITE_THREADS, std::vector<ServerCfg>{std::move(aionCfg), std::move(gsCfg)}, DISCONNECT_THREADS);
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
	s.instance->shutdown();
	stopExecutors(s);
	s.instance.reset();
}

std::vector<commons::utils::InetSocketAddress> getBoundAddresses() {
	State& s = state();
	std::lock_guard lock(s.mutex);
	return s.instance ? s.instance->getBoundAddresses() : std::vector<commons::utils::InetSocketAddress>{};
}

} // namespace aion::loginserver::network::NetConnector
