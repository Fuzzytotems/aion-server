#pragma once

#include <string>
#include <string_view>

/**
 * This namespace provides useful methods to use with accounts.
 * <p>
 * Java: com.aionemu.loginserver.utils.AccountUtils
 *
 * @author SoulKeeper
 */
namespace aion::loginserver::utils::AccountUtils {

/**
 * Encodes password. SHA-1 is used to encode password bytes (UTF-8), Base64 wraps SHA1-hash to string. Thread safe.
 *
 * @param password password to encode (UTF-8)
 * @return the encoded password (28 characters)
 * @throws commons::utils::Exception "Exception while encoding password" if OpenSSL fails (Java: Error on NoSuchAlgorithmException)
 */
std::string encodePassword(std::string_view password);

} // namespace aion::loginserver::utils::AccountUtils
