#include "aion/commons/utils/InetSocketAddress.h"

#include <optional>
#include <system_error>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils {

namespace {

/** Java: the address of an IP literal, with IPv4-mapped IPv6 addresses converted to IPv4 (like Java's InetAddress) */
std::optional<asio::ip::address> parseIpLiteral(const std::string& host) noexcept {
	std::error_code error;
	asio::ip::address address = asio::ip::make_address(host, error);
	if (error)
		return std::nullopt;
	if (address.is_v6() && address.to_v6().is_v4_mapped())
		return asio::ip::make_address_v4(asio::ip::v4_mapped, address.to_v6());
	return address;
}

std::vector<uint8_t> toBytes(const asio::ip::address& address) {
	if (address.is_v4()) {
		auto bytes = address.to_v4().to_bytes();
		return {bytes.begin(), bytes.end()};
	}
	auto bytes = address.to_v6().to_bytes();
	return {bytes.begin(), bytes.end()};
}

} // namespace

bool InetSocketAddress::isAnyLocalAddress() const noexcept {
	if (host.empty())
		return true;
	std::optional<asio::ip::address> address = parseIpLiteral(host);
	return address && address->is_unspecified();
}

std::vector<uint8_t> InetSocketAddress::resolveAddressBytes() const {
	if (host.empty())
		return {0, 0, 0, 0};
	if (std::optional<asio::ip::address> address = parseIpLiteral(host))
		return toBytes(*address);
	try {
		asio::io_context context;
		asio::ip::tcp::resolver resolver(context);
		std::optional<asio::ip::address> first;
		for (const auto& entry : resolver.resolve(host, "")) {
			asio::ip::address address = entry.endpoint().address();
			if (address.is_v6() && address.to_v6().is_v4_mapped())
				address = asio::ip::make_address_v4(asio::ip::v4_mapped, address.to_v6());
			if (address.is_v4()) // java.net.preferIPv6Addresses=false: IPv4 addresses first
				return toBytes(address);
			if (!first)
				first = address;
		}
		if (first)
			return toBytes(*first);
	} catch (const std::exception&) {
		throw IOException(host + ": Name or service not known", std::current_exception()); // Java: UnknownHostException
	}
	throw IOException(host + ": Name or service not known");
}

} // namespace aion::commons::utils
