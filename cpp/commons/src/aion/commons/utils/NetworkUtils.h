#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/utils/ByteBuffer.h"

namespace aion::commons::utils::NetworkUtils {

/**
 * @return The outbound IPv4 address (the local address used to reach 1.1.1.1), or nullopt if unavailable.
 */
std::optional<std::string> findLocalIPv4();

/**
 * Checks if an IPv4 address matches a pattern like "*.*.*.*", "*" or "192.168.1.0-255".
 * <p>
 * Deviation: Java parses range bounds and octets as signed bytes, so octets above 127 throw a NumberFormatException. Here they are parsed as
 * 0-255; malformed input returns false.
 */
bool checkIPMatching(std::string_view pattern, std::string_view address);

/**
 * @return The IP as a human-readable string (i.e. 127.0.0.1). The lowest byte of ip is the first octet.
 */
std::string intToIpString(int32_t ip);

/**
 * @return Formatted hex dump of the buffer's data from index 0 to its limit.
 */
std::string toHex(const ByteBuffer& buffer);

/**
 * @param start position to start reading from
 * @param end end position (exclusive)
 * @return Formatted hex dump like "0000: 01 02 ... ascii", 16 bytes per row. Empty if start &gt;= end.
 * @throws IndexOutOfBoundsException if start &lt; end and the range is not within [0, limit) (Java: ByteBuffer.get(index) throws)
 */
std::string toHex(const ByteBuffer& buffer, int32_t start, int32_t end);

} // namespace aion::commons::utils::NetworkUtils
