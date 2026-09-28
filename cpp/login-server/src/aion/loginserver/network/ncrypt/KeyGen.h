#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "aion/loginserver/network/ncrypt/EncryptedRSAKeyPair.h"

/**
 * Key generator for the client connection cryptography: Blowfish session keys and a cache of RSA key pairs.
 * <p>
 * Thread-safe. init() is called once at startup (LoginServer), the other functions per connection (LoginConnection.initialized).
 * <p>
 * Java: com.aionemu.loginserver.network.ncrypt.KeyGen (a class with static members only)
 *
 * @author -Nemesiss-
 */
namespace aion::loginserver::network::ncrypt::KeyGen {

/** Size of a Blowfish session key in bytes (Java: KeyGenerator "Blowfish" default of 128 bits) */
inline constexpr size_t BLOWFISH_KEY_SIZE = 16;

/** Raw Blowfish session key (Java: SecretKey.getEncoded()) */
using BlowfishKey = std::array<uint8_t, BLOWFISH_KEY_SIZE>;

/** Number of cached RSA key pairs */
inline constexpr size_t RSA_KEY_PAIR_COUNT = 10;

/**
 * Generates the 10 cached RSA key pairs (1024 bits, public exponent 65537 = RSAKeyGenParameterSpec.F4). Calling it again replaces the cache;
 * pairs already handed out stay valid.
 *
 * @throws OpenSslException if key generation fails (Java: throw new Error(e))
 */
void init();

/**
 * @return a new random Blowfish key of 16 bytes from OpenSSL's CSPRNG (RAND_bytes)
 * @throws utils::IllegalStateException if init() was not called yet (Java: NullPointerException)
 * @throws OpenSslException if the random generator fails
 */
BlowfishKey generateBlowfishKey();

/**
 * @return one of the cached RSA key pairs, chosen with utils::Rnd
 * @throws utils::IllegalStateException if init() was not called yet (Java: returns null, which causes a NullPointerException in SM_INIT)
 */
std::shared_ptr<const EncryptedRSAKeyPair> getEncryptedRSAKeyPair();

} // namespace aion::loginserver::network::ncrypt::KeyGen
