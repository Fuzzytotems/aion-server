#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "aion/loginserver/network/ncrypt/OpenSslUtils.h"

namespace aion::loginserver::network::ncrypt {

/**
 * An RSA key pair whose public modulus N is additionally available in a scrambled form (encryptModulus) to be sent to the client in SM_INIT.
 * The client unscrambles it and RSA-encrypts the credentials of CM_LOGIN with (e = 65537, N).
 * <p>
 * Immutable after construction, so one instance can be shared by all connections and threads (OpenSSL 3 key objects support concurrent
 * operations; each decrypt uses its own operation context).
 * <p>
 * Java: com.aionemu.loginserver.network.ncrypt.EncryptedRSAKeyPair. The private key decryption, which Java's CM_LOGIN performs with a
 * "RSA/ECB/nopadding" Cipher, is available as decrypt().
 *
 * @author EvilSpirit
 */
class EncryptedRSAKeyPair {
public:
	/** Size of the scrambled modulus and of one RSA block of a 1024-bit key, in bytes */
	static constexpr size_t RSA_BLOCK_SIZE = 128;

	/**
	 * Stores the RSA key pair and scrambles its modulus.
	 *
	 * @throws utils::IllegalArgumentException if the key is null, not an RSA key, or its modulus is shorter than 128 bytes in two's complement
	 * representation (Java: ArrayIndexOutOfBoundsException)
	 */
	explicit EncryptedRSAKeyPair(EvpPkeyPtr rsaKeyPair);

	/** @return the RSA key pair (owned by this object, never null) */
	EVP_PKEY* getRSAKeyPair() const noexcept { return rsaKeyPair.get(); }

	/** @return the scrambled modulus to be transferred to the client (128 bytes for a 1024-bit key) */
	std::span<const uint8_t> getEncryptedModulus() const noexcept { return encryptedModulus; }

	/**
	 * Decrypts data with the private key in 128-byte blocks without padding (raw RSA, Java: Cipher "RSA/ECB/nopadding" with doFinal per block
	 * as in CM_LOGIN.decryptLoginData). Each decrypted block is left-padded with zeros to 128 bytes.
	 *
	 * @param encryptedData
	 *          a multiple of 128 bytes (empty data gives an empty result)
	 * @return the decrypted data (same size as the input), or nullopt where Java's Cipher throws a GeneralSecurityException: a block is not
	 * smaller than the modulus, the key size is not 128 bytes, or OpenSSL fails otherwise (the OpenSSL error queue is cleared)
	 * @throws utils::IllegalArgumentException if the size is not a multiple of 128 (Java: IllegalArgumentException "Bad arguments" from
	 * Cipher.doFinal for the incomplete last block)
	 */
	std::optional<std::vector<uint8_t>> decrypt(std::span<const uint8_t> encryptedData) const;

	/**
	 * Scrambles an RSA modulus. Java: private encryptModulus(BigInteger).
	 * <p>
	 * The modulus is first converted like BigInteger.toByteArray() (minimal big endian two's complement, i.e. with a leading 0x00 if the top bit
	 * of the magnitude is set). A 129-byte array with a leading 0x00 is shortened to 128 bytes. Then bytes 0-3 are swapped with 0x4D-0x50,
	 * bytes 0x00-0x3F are XORed with 0x40-0x7F, bytes 0x0D-0x10 with 0x34-0x37, and bytes 0x40-0x7F with 0x00-0x3F. Bytes beyond 0x80 (only
	 * for moduli longer than 1024 bits) are kept.
	 *
	 * @param modulus
	 *          the modulus as unsigned big endian magnitude (leading zero bytes are ignored)
	 * @throws utils::IllegalArgumentException if the two's complement representation is shorter than 128 bytes
	 */
	static std::vector<uint8_t> encryptModulus(std::span<const uint8_t> modulus);

private:
	EvpPkeyPtr rsaKeyPair;
	std::vector<uint8_t> encryptedModulus;
};

} // namespace aion::loginserver::network::ncrypt
