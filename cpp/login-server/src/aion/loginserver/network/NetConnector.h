#pragma once

#include <vector>

#include "aion/commons/utils/InetSocketAddress.h"

/**
 * The network of the login server: a NioServer listening on Config::CLIENT_SOCKET_ADDRESS for Aion game clients (LoginConnection) and on
 * Config::GAMESERVER_SOCKET_ADDRESS for game servers (GsConnection), plus the executors the connections use:
 * <ul>
 * <li>the Aion client packet processor (Java: LoginConnection's static PacketProcessor(1, 8, 50, 3))</li>
 * <li>the game server packet processor (Java: GsConnection.PACKET_EXECUTOR, a cached thread pool; see GsConnection)</li>
 * <li>the ping scheduler (Java: GsConnection.PINGPONG_EXECUTOR)</li>
 * <li>DISCONNECT_THREADS threads running onDisconnect (Java: a cached thread pool); LoginConnection::onDisconnect writes to the database</li>
 * </ul>
 * Thread safe; connect and shutdown must not be called concurrently with each other.
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

/** Shuts the server down (see NioServer::shutdown), then the packet processors and the ping scheduler. Does nothing if not connected. */
void shutdown();

/**
 * C++ addition for tests: the addresses the server is bound to (client address first, game server address second), empty if not connected.
 */
std::vector<commons::utils::InetSocketAddress> getBoundAddresses();

} // namespace aion::loginserver::network::NetConnector
