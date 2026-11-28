#pragma once

#include <vector>

#include "aion/commons/utils/InetSocketAddress.h"

/**
 * The network of the login server: one server listening on Config::CLIENT_SOCKET_ADDRESS for Aion game clients (LoginConnection, packets up to
 * 8192 * 2 bytes) and on Config::GAMESERVER_SOCKET_ADDRESS for game servers (GsConnection, packets up to 8192 * 8 bytes), plus the executors the
 * connections use:
 * <ul>
 * <li>the client packet executors (Java: DefaultEventExecutorGroup(8, "PacketProcessor")), which run the packets of a client one after another</li>
 * <li>the game server packet executors (Java: GsConnection.PACKET_EXECUTOR, a cached thread pool; see GsConnection)</li>
 * <li>the ping scheduler (Java: GsConnection.PINGPONG_EXECUTOR)</li>
 * <li>DISCONNECT_THREADS threads of the commons server, which hand the disconnects to the packet executors</li>
 * </ul>
 * C++: a commons NioServer (asio) with max(1, NIO_READ_WRITE_THREADS) IO threads that accept and do the IO alike (Java since upstream
 * bce89b2fc: Netty with one acceptor thread and max(1, NIO_READ_WRITE_THREADS) I/O threads, epoll on Linux). Thread safe; connect and shutdown
 * must not be called concurrently with each other.
 * <p>
 * Java: com.aionemu.loginserver.network.NetConnector
 *
 * @author KID, Neon
 */
namespace aion::loginserver::network::NetConnector {

/** Number of threads executing onDisconnect callbacks. */
inline constexpr int32_t DISCONNECT_THREADS = 8;

/**
 * Creates the server from the current Config (Java: in the static initializer) and starts listening.
 * C++ addition: after shutdown() it may be called again, creating a new server (used by tests).
 *
 * @throws commons::utils::IOException if a socket cannot be bound
 * @throws commons::utils::IllegalStateException if already connected
 */
void connect();

/**
 * Closes all connections and the server sockets (see NioServer::shutdown), lets the client packet executors run what is queued (at most 3 seconds),
 * then stops the ping scheduler and the game server packet executors (at most 1 second each). Does nothing if not connected.
 */
void shutdown();

/**
 * C++ addition for tests: the addresses the server is bound to (client address first, game server address second), empty if not connected.
 */
std::vector<commons::utils::InetSocketAddress> getBoundAddresses();

} // namespace aion::loginserver::network::NetConnector
