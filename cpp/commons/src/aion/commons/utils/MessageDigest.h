#pragma once

#include <cstddef>
#include <string_view>

namespace aion::commons::utils::MessageDigest {

/**
 * Compares two byte sequences (e.g. a password with the configured one, as UTF-8 bytes) in time that depends only on their lengths, not on where
 * they differ.
 * <p>
 * Java: java.security.MessageDigest.isEqual(byte[], byte[])
 */
inline bool isEqual(std::string_view a, std::string_view b) noexcept {
	if (a.size() != b.size())
		return false;
	unsigned char result = 0;
	for (size_t i = 0; i < a.size(); i++)
		result |= static_cast<unsigned char>(a[i] ^ b[i]);
	return result == 0;
}

} // namespace aion::commons::utils::MessageDigest
