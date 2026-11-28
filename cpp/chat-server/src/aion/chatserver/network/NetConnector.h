#pragma once

#include <memory>
#include <vector>

#include "aion/commons/utils/InetSocketAddress.h"

namespace aion::commons::network::netty {
class PacketExecutorGroup;
}

/**
 * The network of the chat server: one server listening on NetworkConfig::CLIENT_SOCKET_ADDRESS for Aion game clients (AionConnection, packets
 * up to 8192 * 2 bytes) and on NetworkConfig::GAMESERVER_SOCKET_ADDRESS for game servers (GsConnection, packets up to 8192 * 8 bytes). The
 * packets of every connection run one after another on the packet executors (8 threads), outside the IO threads.
 * <p>
 * C++: a commons NioServer (asio) with max(1, NIO_READ_WRITE_THREADS) IO threads, which accept and do the IO alike (Java: Netty with one
 * acceptor thread and max(1, NIO_READ_WRITE_THREADS) I/O threads, using epoll on Linux). Thread safe; connect and shutdown must not be called
 * concurrently with each other.
 * <p>
 * Java: com.aionemu.chatserver.network.NetConnector
 */
namespace aion::chatserver::network::NetConnector {

/**
 * Creates the server from the current NetworkConfig and starts listening ("Listening on ... for Aion game clients", "Listening on ... for game
 * servers"). C++ addition: after shutdown() it may be called again, creating a new server (used by tests).
 *
 * @throws commons::utils::IOException if a socket cannot be bound
 * @throws commons::utils::IllegalStateException if already connected
 */
void connect();

/**
 * Closes all connections and the server sockets, then lets the packet executors run the packets that are left (at most 3 seconds). Does
 * nothing if not connected.
 */
void shutdown();

/** C++ addition for tests: the bound addresses, the client address first, the game server address second (empty if not connected). */
std::vector<commons::utils::InetSocketAddress> getBoundAddresses();

/** C++ addition for tests: the packet executors (nullptr if not connected). */
std::shared_ptr<commons::network::netty::PacketExecutorGroup> getPacketExecutors();

} // namespace aion::chatserver::network::NetConnector
