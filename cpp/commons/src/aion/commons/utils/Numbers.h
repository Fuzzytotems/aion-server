#pragma once

#include <cstdint>
#include <string_view>

namespace aion::commons::utils {

/**
 * Java: Integer.parseInt(s, radix) - an optional sign ('+' or '-') followed by at least one digit of the radix, nothing else (no whitespace,
 * no "0x" prefix; leading zeros are decimal, unlike NumberParser::decodeInt which follows Integer.decode).
 * <p>
 * Deviation: only ASCII digits and letters are accepted (Java: all Unicode digits).
 *
 * @throws NumberFormatException with Java's messages, e.g. <tt>For input string: "12a"</tt> (plus <tt> under radix 16</tt> for other radixes)
 */
int32_t parseInt(std::string_view s, int32_t radix = 10);

/** Java: Long.parseLong(s, radix), see parseInt. */
int64_t parseLong(std::string_view s, int32_t radix = 10);

} // namespace aion::commons::utils
