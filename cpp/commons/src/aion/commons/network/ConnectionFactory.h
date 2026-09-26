#pragma once

#include <functional>
#include <memory>

#include <asio/ip/tcp.hpp>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network {

class AConnectionBase;
class NioServer;

/**
 * Factory for connection implementations, used by NioServer to create a connection for every accepted socket.
 * <p>
 * The factory receives the connected socket (TCP_NODELAY already set) and the server, which every AConnection constructor needs:
 * <pre>
 * ServerCfg cfg{address, "Aion game clients", [](asio::ip::tcp::socket socket, NioServer& server) {
 * 	return std::make_shared&lt;LoginConnection&gt;(std::move(socket), server);
 * }};
 * </pre>
 * Returning nullptr rejects the connection: the socket is closed (Java: GameConnectionFactoryImpl returns null for flooding IPs).
 * Connections must be created with std::make_shared (they use enable_shared_from_this). Exceptions thrown by the factory are logged and the
 * socket is closed.
 * <p>
 * Java: com.aionemu.commons.network.ConnectionFactory
 *
 * @author -Nemesiss-
 */
using ConnectionFactory = std::function<std::shared_ptr<AConnectionBase>(asio::ip::tcp::socket socket, NioServer& server)>;

} // namespace aion::commons::network
