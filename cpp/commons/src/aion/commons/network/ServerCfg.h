#pragma once

#include <cstdint>
#include <string>

#include "aion/commons/network/ConnectionFactory.h"
#include "aion/commons/utils/InetSocketAddress.h"

namespace aion::commons::network {

/**
 * Configuration of one listening socket of a NioServer: the address to bind, a description for log messages and the factory creating
 * connections for accepted sockets.
 * <p>
 * Java: com.aionemu.commons.network.ServerCfg
 *
 * @author -Nemesiss-, Neon
 */
struct ServerCfg {
	utils::InetSocketAddress address;
	std::string clientDescription;
	/** May be empty if the config is only used for its address (chat server: Netty client config). */
	ConnectionFactory connectionFactory;

	bool isAnyLocalAddress() const noexcept { return address.isAnyLocalAddress(); }

	/** Deviation: Java returns the resolved IP; the address is kept unresolved here, so this is the configured host (name or IP literal). */
	const std::string& getIP() const noexcept { return address.host; }

	int32_t getPort() const noexcept { return address.port; }

	/** @return "all addresses on port 2106" or "127.0.0.1:2106" */
	std::string getAddressInfo() const { return address.getAddressInfo(); }
};

} // namespace aion::commons::network
