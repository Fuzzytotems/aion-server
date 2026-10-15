#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "aion/chatserver/network/gameserver/GsConnection.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/utils/InetSocketAddress.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network {
struct ServerCfg;
}

namespace aion::chatserver::network::netty {

namespace pipeline {
class ExecutionHandler;
class LoginToClientPipeLineFactory;
} // namespace pipeline

/**
 * The network of the chat server: a server for the Aion clients on NetworkConfig::CLIENT_SOCKET_ADDRESS (Java: Netty) and one for the game
 * server on NetworkConfig::GAMESERVER_SOCKET_ADDRESS (Java: commons' NioServer), both created and listening from the constructor on, like Java's
 * singleton.
 * <p>
 * C++: the client side is a commons NioServer as well, with NIO_READ_WRITE_THREADS + 1 IO threads (Java: Netty's NIO_READ_WRITE_THREADS + 1
 * worker threads besides its boss thread) and ClientChannelHandler connections, see there for how Netty's pipeline maps onto it. The game server
 * side has NIO_READ_WRITE_THREADS IO threads (at least 1), one thread for onDisconnect (Java: a single thread executor) and a PacketProcessor for
 * the received packets (see GsConnection).
 * <p>
 * Java: com.aionemu.chatserver.network.netty.NettyServer
 *
 * @author ATracer, Neon
 */
class NettyServer {
public:
	/** Java: getInstance() - the server of the running chat server, created on the first call. */
	static NettyServer& getInstance();

	/**
	 * C++ addition (Java: the private constructor of the singleton): creates both servers from the current NetworkConfig and starts listening
	 * ("Listening on ... for Aion game clients", "Listening on ... for game servers"). Tests create their own instances.
	 *
	 * @throws commons::utils::IOException if a socket cannot be bound
	 */
	NettyServer();

	/** Calls shutdownAll(). */
	~NettyServer();

	NettyServer(const NettyServer&) = delete;
	NettyServer& operator=(const NettyServer&) = delete;

	/**
	 * Shuts both servers down: the client server (closing every client connection; then the queued client events are run, waiting up to 5
	 * seconds, and an event still running after that is left to end on its own), then the game server server and its packet processor. Calling it
	 * again does nothing.
	 */
	void shutdownAll();

	/** C++ addition for tests: the bound addresses, the client address first, the game server address second (empty after shutdownAll()). */
	std::vector<commons::utils::InetSocketAddress> getBoundAddresses() const;

	/** C++ addition for tests: the executor of the client events (nullptr after shutdownAll()). */
	std::shared_ptr<pipeline::ExecutionHandler> getExecutionHandler() const;

private:
	/** Java: initChannel(gameClientConfig) - binds the client server */
	std::unique_ptr<commons::network::NioServer> initChannel(commons::network::ServerCfg gameClientConfig);

	mutable std::mutex mutex;
	std::shared_ptr<pipeline::LoginToClientPipeLineFactory> pipelineFactory;
	/** Java: aionClientChannelFactory and aionClientChannelGroup */
	std::unique_ptr<commons::network::NioServer> aionClientServer;
	/** Java: GsConnection.PACKET_EXECUTOR */
	std::shared_ptr<gameserver::GsConnection::Processor> gsPacketExecutor;
	std::unique_ptr<commons::network::NioServer> nioServer;
};

} // namespace aion::chatserver::network::netty
