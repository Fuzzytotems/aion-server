#include "aion/gameserver/services/player/SecurityTokenService.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>

#include "aion/gameserver/model/account/Account.h"

namespace aion::gameserver::services::player {

/** Java Base64.getEncoder().encodeToString(bytes): RFC 4648 alphabet with '=' padding, no line breaks */
static std::string encodeBase64(const std::array<uint8_t, 16>& bytes) {
	static constexpr std::string_view ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string encoded;
	encoded.reserve((bytes.size() + 2) / 3 * 4);
	for (size_t i = 0; i < bytes.size(); i += 3) {
		const size_t remaining = bytes.size() - i;
		uint32_t chunk = static_cast<uint32_t>(bytes[i]) << 16;
		if (remaining > 1)
			chunk |= static_cast<uint32_t>(bytes[i + 1]) << 8;
		if (remaining > 2)
			chunk |= bytes[i + 2];
		encoded.push_back(ALPHABET[(chunk >> 18) & 0x3F]);
		encoded.push_back(ALPHABET[(chunk >> 12) & 0x3F]);
		encoded.push_back(remaining > 1 ? ALPHABET[(chunk >> 6) & 0x3F] : '=');
		encoded.push_back(remaining > 2 ? ALPHABET[chunk & 0x3F] : '=');
	}
	return encoded;
}

void SecurityTokenService::generateToken(model::account::Account& account) {
	// Java SecureRandom: MSVC's std::random_device draws from the operating system's cryptographically secure generator (rand_s)
	std::random_device secureRandom;
	std::uniform_int_distribution<int> byteDistribution(0, 255);
	std::array<uint8_t, 16> token{};
	for (uint8_t& b : token)
		b = static_cast<uint8_t>(byteDistribution(secureRandom));
	account.setSecurityToken(encodeBase64(token));
}

} // namespace aion::gameserver::services::player
