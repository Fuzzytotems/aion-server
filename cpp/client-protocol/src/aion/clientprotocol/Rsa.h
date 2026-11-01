#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace aion::clientprotocol {

/**
 * Raw RSA (no padding) with numbers of at most 1024 bits, as the Aion login uses it: CM_LOGIN carries the credentials encrypted with the public
 * key (e = 65537) whose modulus SM_INIT sends. Replaces OpenSSL's BN_mod_exp in the client side of the protocol, so the library needs nothing
 * but the standard library. Numbers are unsigned big endian byte strings, like OpenSSL's BN_bin2bn/BN_bn2binpad.
 * <p>
 * This is a public key operation on public data, so it is not constant time.
 */
namespace rsa {

inline constexpr size_t BLOCK_SIZE = 128;
inline constexpr uint32_t PUBLIC_EXPONENT = 65537;

/**
 * @return base^exponent mod modulus, left-padded to 128 bytes
 * @param modulus
 *          at most 128 significant bytes (leading zero bytes are ignored), not 0
 * @param base
 *          at most 128 significant bytes; it must be smaller than the modulus
 * @throws std::invalid_argument for a modulus of 0 or with more than 128 significant bytes, or a base that is not smaller than the modulus
 */
std::array<uint8_t, BLOCK_SIZE> modPow(std::span<const uint8_t> base, uint32_t exponent, std::span<const uint8_t> modulus);

/**
 * Raw RSA encryption (c = m^65537 mod n) of each 128-byte block, without padding.
 *
 * @param plain
 *          a multiple of 128 bytes; each block (big endian) must be smaller than the modulus
 * @return the encrypted blocks, each left-padded to 128 bytes
 * @throws std::invalid_argument if plain is no multiple of 128 bytes, or for the cases of modPow
 */
std::vector<uint8_t> encrypt(std::span<const uint8_t> modulus, std::span<const uint8_t> plain);

} // namespace rsa

} // namespace aion::clientprotocol
