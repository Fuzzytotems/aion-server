#pragma once

#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/commons/utils/InetSocketAddress.h"

namespace aion::commons::configuration::transformers {

namespace InetSocketAddressTransformer {

/**
 * Parses {@code host:port} with the acceptance rules of Java's {@code new InetSocketAddress(new URI(null, value, null, null, null).getHost(),
 * uri.getPort())}:
 * <ul>
 * <li>host is an IPv4 address (four dot-separated decimal numbers &lt;= 255), a hostname (RFC 2396: ASCII letters, digits and inner hyphens, dot
 * separated labels, the last label of a multi-label name starts with a letter) or an IPv6 address in square brackets (optionally with a
 * %scope_id), which is stored without the brackets</li>
 * <li>an optional "userinfo@" prefix is accepted and ignored, like in Java</li>
 * <li>the port is mandatory and must be 0..65535</li>
 * </ul>
 * Deviation: the host is not resolved (Java resolves it eagerly if possible; utils::InetSocketAddress is resolved when binding or connecting).
 * So that utils::InetSocketAddress::isAnyLocalAddress() agrees with Java's getAddress().isAnyLocalAddress(), every other spelling of the
 * unspecified IPv6 address (e.g. "[0:0:0:0:0:0:0:0]", "[::0.0.0.0]") is stored as "::" and the IPv4-mapped unspecified address
 * ("[::ffff:0.0.0.0]", an Inet4Address in Java) as "0.0.0.0". Other hosts keep their spelling.
 * Error messages follow Java for the common cases ("port out of range:-1" if the port is missing, "hostname can't be null" for an invalid host);
 * for invalid bracketed IPv6 addresses Java reports various URISyntaxException messages, which are summarized in one message here.
 *
 * @throws utils::IllegalArgumentException if the value is invalid
 */
utils::InetSocketAddress parse(std::string_view value);

} // namespace InetSocketAddressTransformer

/**
 * Transforms strings in the format {@code host:port} to an InetSocketAddress, where host can be a hostname or an IP address (IPv6 addresses must be
 * enclosed in square brackets). See InetSocketAddressTransformer::parse.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.InetSocketAddressTransformer
 *
 * @author SoulKeeper
 */
template <>
struct PropertyTransformer<utils::InetSocketAddress> {
	static std::string typeName() { return "InetSocketAddress"; }

	static utils::InetSocketAddress parseObject(std::string_view value) { return InetSocketAddressTransformer::parse(value); }
};

} // namespace aion::commons::configuration::transformers
