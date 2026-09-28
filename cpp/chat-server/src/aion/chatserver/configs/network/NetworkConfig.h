#pragma once

#include <cstdint>
#include <string>

#include "aion/commons/utils/InetSocketAddress.h"

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::chatserver::configs::network {

/**
 * The chat server's network settings (config/network/network.properties). Bound once at startup (see configs::Config), read afterwards.
 * <p>
 * Java: com.aionemu.chatserver.configs.network.NetworkConfig
 */
struct NetworkConfig {
	/**
	 * Address where Aion clients will attempt to connect to (host/domain name or IP). Sent to the game server in SM_GS_AUTH_RESPONSE, which
	 * passes it on to its clients. Config::load replaces 0.0.0.0 by a local IPv4 address.
	 */
	static inline commons::utils::InetSocketAddress CLIENT_CONNECT_ADDRESS;

	/** Local address where CS will listen for Aion client connections (0.0.0.0 = bind any local IP) */
	static inline commons::utils::InetSocketAddress CLIENT_SOCKET_ADDRESS;

	/** Local address where CS will listen for GS connections (0.0.0.0 = bind any local IP) */
	static inline commons::utils::InetSocketAddress GAMESERVER_SOCKET_ADDRESS;

	/** Password for GS authentication */
	static inline std::string GAMESERVER_PASSWORD;

	/**
	 * Number of threads dedicated to be doing io read & write. There is always 1 acceptor thread. If value is < 1 - acceptor thread will also
	 * handle read & write. If value is > 0 - there will be given amount of read & write threads + 1 acceptor thread.
	 */
	static inline int32_t NIO_READ_WRITE_THREADS = 0;

	/** Binds the fields above to their property keys (Java: the @Property annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::chatserver::configs::network
