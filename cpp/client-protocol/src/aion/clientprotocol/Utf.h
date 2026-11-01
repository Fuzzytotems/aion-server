#pragma once

#include <string>
#include <string_view>

namespace aion::clientprotocol {

// UTF-16 <-> UTF-8 conversion for the strings of the Aion protocol (UTF-16LE on the wire, UTF-8 in this library's structs). The library's own
// replacement of commons::utils::StringUtils::toUtf8/toUtf16, with the same handling of well-formed input.

/** Converts UTF-16 to UTF-8. An unpaired surrogate becomes U+FFFD (Java: new String(chars).getBytes(UTF_8)) */
std::string utf16ToUtf8(std::u16string_view utf16);

/**
 * Converts UTF-8 to UTF-16. Malformed input (a stray continuation byte, an overlong form, an encoded surrogate, a truncated sequence) becomes
 * one U+FFFD per malformed byte sequence start.
 */
std::u16string utf8ToUtf16(std::string_view utf8);

} // namespace aion::clientprotocol
