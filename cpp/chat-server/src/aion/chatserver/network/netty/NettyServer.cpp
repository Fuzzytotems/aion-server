#include "aion/chatserver/network/netty/NettyServer.h"

#include <chrono>
#include <utility>

#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/chatserver/network/aion/ClientPacketHandler.h"
#include "aion/chatserver/network/gameserver/GsConnectionFactoryImpl.h"
#include "aion/chatserver/network/netty/pipeline/LoginToClientPipeLineFactory.h"
#include "aion/commons/network/ServerCfg.h"

namespace aion::chatserver::network::netty {

using commons::network::NioServer;
using commons::network::ServerCfg;
using configs::network::NetworkConfig;

namespace {

/** how long shutdownAll waits for the events of the closed client connections */
constexpr std::chrono::seconds CLIENT_EVENTS_TIMEOUT{5};

} // namespace

NettyServer& NettyServer::getInstance() {
	static auto* instance = new NettyServer(); // leaked: shut down by the shutdown hook (ChatServer::shutdown), connections may outlive statics
	return *instance;
}

NettyServer::NettyServer() {
	try {
		pipelineFactory = std::make_shared<pipeline::LoginToClientPipeLineFactory>(std::make_shared<const aion::ClientPacketHandler>());
		aionClientServer = initChannel(ServerCfg{NetworkConfig::CLIENT_SOCKET_ADDRESS, "Aion game clients", {}});

		// Deviation: Java runs game server packets on a cached thread pool (unordered), see GsConnection
		gsPacketExecutor = std::make_shared<gameserver::GsConnection::Processor>(1, 1, 50, 3);
		nioServer = std::make_unique<NioServer>(NetworkConfig::NIO_READ_WRITE_THREADS,
			std::vector<ServerCfg>{ServerCfg{NetworkConfig::GAMESERVER_SOCKET_ADDRESS, "game servers", gameserver::GsConnectionFactoryImpl(gsPacketExecutor)}}, 1);
		nioServer->connect(); // Java: connect(Executors.newSingleThreadExecutor()), here the server's own disconnect thread
	} catch (...) {
		shutdownAll();
		throw;
	}
}

NettyServer::~NettyServer() {
	shutdownAll();
}

std::unique_ptr<NioServer> NettyServer::initChannel(ServerCfg gameClientConfig) {
	gameClientConfig.connectionFactory = [factory = pipelineFactory](asio::ip::tcp::socket socket, NioServer& server) {
		return factory->getPipeline(std::move(socket), server);
	};
	auto server = std::make_unique<NioServer>(NetworkConfig::NIO_READ_WRITE_THREADS + 1, std::vector<ServerCfg>{std::move(gameClientConfig)}, 1);
	server->connect(); // logs "Listening on <address info> for Aion game clients"
	return server;
}

void NettyServer::shutdownAll() {
	std::unique_ptr<NioServer> clientServer;
	std::shared_ptr<pipeline::LoginToClientPipeLineFactory> factory;
	std::unique_ptr<NioServer> gsServer;
	std::shared_ptr<gameserver::GsConnection::Processor> gsProcessor;
	{
		std::lock_guard lock(mutex);
		clientServer = std::move(aionClientServer);
		factory = std::move(pipelineFactory);
		gsServer = std::move(nioServer);
		gsProcessor = std::move(gsPacketExecutor);
	}
	if (clientServer)
		clientServer->shutdown();
	if (factory)
		factory->shutdown(CLIENT_EVENTS_TIMEOUT);
	if (gsServer)
		gsServer->shutdown();
	if (gsProcessor)
		gsProcessor->shutdown();
}

std::vector<commons::utils::InetSocketAddress> NettyServer::getBoundAddresses() const {
	std::lock_guard lock(mutex);
	std::vector<commons::utils::InetSocketAddress> addresses;
	if (aionClientServer) {
		for (const auto& address : aionClientServer->getBoundAddresses())
			addresses.push_back(address);
	}
	if (nioServer) {
		for (const auto& address : nioServer->getBoundAddresses())
			addresses.push_back(address);
	}
	return addresses;
}

std::shared_ptr<pipeline::ExecutionHandler> NettyServer::getExecutionHandler() const {
	std::lock_guard lock(mutex);
	return pipelineFactory ? pipelineFactory->getExecutionHandler() : nullptr;
}

} // namespace aion::chatserver::network::netty
