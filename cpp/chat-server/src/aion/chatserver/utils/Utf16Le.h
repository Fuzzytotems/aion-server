#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * Conversions between UTF-16LE byte arrays and strings with Java's rules. The chat server keeps many client texts as raw UTF-16LE bytes (the
 * channel identifier, the "name@identifier" of a player, chat messages) and converts them with new String(bytes, StandardCharsets.UTF_16LE)
 * where it needs a String; these helpers reproduce that conversion exactly, so malformed client input ends up as the same characters as in
 * Java.
 * <p>
 * C++ helper (no Java class of its own); proposed for commons (StringUtils) since the game server reads such arrays too.
 */
namespace aion::chatserver::utils::Utf16Le {

/**
 * Java: new String(bytes, StandardCharsets.UTF_16LE), i.e. sun.nio.cs.UnicodeDecoder with CodingErrorAction.REPLACE:
 * <ul>
 * <li>a high surrogate followed by a low surrogate is kept as a pair</li>
 * <li>a high surrogate followed by any other char is malformed for 4 bytes: both chars become one U+FFFD</li>
 * <li>an unpaired low surrogate and U+FFFE (a reversed byte order mark) become U+FFFD</li>
 * <li>the bytes left at the end of the input (an odd last byte, or a high surrogate without its second char) become one U+FFFD</li>
 * </ul>
 * The result never contains an unpaired surrogate.
 */
std::u16string decode(std::span<const uint8_t> bytes);

/** decode() converted to UTF-8, which is lossless since the decoded text has no unpaired surrogates */
std::string newString(std::span<const uint8_t> bytes);

/** Java: String.getBytes(StandardCharsets.UTF_16LE) for a UTF-8 string (malformed UTF-8 becomes U+FFFD, see StringUtils::toUtf16) */
std::vector<uint8_t> getBytes(std::string_view utf8);

} // namespace aion::chatserver::utils::Utf16Le
