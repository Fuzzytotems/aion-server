#include "aion/loginserver/utils/AccountUtils.h"

#include <array>

#include <openssl/evp.h>

#include "aion/commons/utils/Exception.h"

namespace aion::loginserver::utils::AccountUtils {

std::string encodePassword(std::string_view password) {
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int digestLength = 0;
	if (EVP_Digest(password.data(), password.size(), digest.data(), &digestLength, EVP_sha1(), nullptr) != 1)
		throw commons::utils::Exception("Exception while encoding password");

	// Base64 without line breaks (Java: Base64.getEncoder()): 4 characters per started 3 bytes, plus the terminating zero EVP_EncodeBlock writes
	std::array<unsigned char, (EVP_MAX_MD_SIZE + 2) / 3 * 4 + 1> encoded{};
	int encodedLength = EVP_EncodeBlock(encoded.data(), digest.data(), static_cast<int>(digestLength));
	if (encodedLength < 0)
		throw commons::utils::Exception("Exception while encoding password");
	return std::string(reinterpret_cast<const char*>(encoded.data()), static_cast<size_t>(encodedLength));
}

} // namespace aion::loginserver::utils::AccountUtils
