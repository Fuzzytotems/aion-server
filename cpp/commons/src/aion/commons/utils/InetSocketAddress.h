#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <fmt/format.h>

namespace aion::commons::utils {

/**
 * Java: java.net.InetSocketAddress, as used by the network configuration ("host:port"). The host is kept unresolved (a hostname or IP literal);
 * it is resolved when a socket is bound or connected, or by resolveAddressBytes().
 */
struct InetSocketAddress {
	/** Hostname or IP literal without brackets (e.g. "0.0.0.0", "localhost", "::1"). Empty means any local address. */
	std::string host;
	uint16_t port = 0;

	/**
	 * Java: getAddress().isAnyLocalAddress() - true if the host is empty or an IP literal of the unspecified address in any spelling ("0.0.0.0",
	 * "::", "0:0:0:0:0:0:0:0", "::0", the IPv4-mapped "::ffff:0.0.0.0", ...). Hostnames are not resolved and give false.
	 */
	bool isAnyLocalAddress() const noexcept;

	/**
	 * Java: getAddress().getAddress() - the raw IP address: 4 bytes for IPv4 (including IPv4-mapped IPv6 literals, which Java converts to
	 * Inet4Address), 16 bytes for IPv6, in network byte order. A hostname is resolved like Java's InetAddress.getByName with the default
	 * preference for IPv4 addresses; an empty host gives the IPv4 wildcard address 0.0.0.0.
	 * <p>
	 * Deviation: Java resolves the host once when the address is created (at config load), here it is resolved on each call.
	 *
	 * @throws IOException if the host cannot be resolved (Java: UnknownHostException, getAddress() then returns null)
	 */
	std::vector<uint8_t> resolveAddressBytes() const;

	/** Java: ServerCfg.getAddressInfo() - "all addresses on port 2106" or "127.0.0.1:2106" */
	std::string getAddressInfo() const {
		return isAnyLocalAddress() ? fmt::format("all addresses on port {}", port) : fmt::format("{}:{}", host, port);
	}

	std::string toString() const {
		return host.find(':') != std::string::npos ? fmt::format("[{}]:{}", host, port) : fmt::format("{}:{}", host, port);
	}

	bool operator==(const InetSocketAddress&) const = default;
};

} // namespace aion::commons::utils
