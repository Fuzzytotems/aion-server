#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace aion::loginserver::network::ncrypt {

/**
 * Blowfish cipher - symmetric 16-round Feistel cipher with 64-bit blocks, 32-448 bit keys and four key dependent 32-bit S-boxes, in ECB mode.
 * <p>
 * The algorithm (key schedule, round function, P-array and S-box constants) is textbook Blowfish, with one difference in how blocks map to
 * bytes: each 8-byte block is read as two <b>little endian</b> 32-bit halves (left = bytes 0-3, right = bytes 4-7) and written back the same
 * way, whereas the reference implementation uses big endian halves. So the Aion ciphertext of a block equals the textbook ciphertext of the
 * block with the byte order of each half reversed (and reversed again). The key bytes are used in their natural order.
 * <p>
 * Keys longer than 72 bytes are accepted, but only the first 72 bytes influence the key schedule (like textbook Blowfish).
 * <p>
 * Not thread-safe: an instance must not be used by several threads at once (CryptEngine serializes access).
 * <p>
 * Java: com.aionemu.loginserver.network.ncrypt.BlowfishCipher
 *
 * @author EvilSpirit
 */
class BlowfishCipher {
public:
	/** Size of one cipher block in bytes */
	static constexpr size_t BLOCK_SIZE = 8;

	/**
	 * Initializes the cipher with the given key.
	 *
	 * @throws utils::IllegalArgumentException if the key is empty (Java: ArrayIndexOutOfBoundsException)
	 */
	explicit BlowfishCipher(std::span<const uint8_t> blowfishKey);

	/**
	 * Replaces the key and reinitializes the P-array and S-boxes.
	 *
	 * @throws utils::IllegalArgumentException if the key is empty (the cipher keeps its previous key)
	 */
	void updateKey(std::span<const uint8_t> blowfishKey);

	/**
	 * Encrypts the data in place, block by block. Only whole 8-byte blocks are processed: trailing bytes (data.size() % 8) stay unchanged.
	 * Java: cipher(data, offset, length) - pass data.subspan(offset, length).
	 */
	void cipher(std::span<uint8_t> data) const noexcept;

	/** Decrypts the data in place, block by block. Trailing bytes (data.size() % 8) stay unchanged. Java: decipher(data, offset, length) */
	void decipher(std::span<uint8_t> data) const noexcept;

private:
	/** The round (Feistel) function */
	uint32_t F(uint32_t x) const noexcept;

	/** Encrypts one block given as its two halves */
	void cipherBlock(uint32_t& xl, uint32_t& xr) const noexcept;

	std::array<uint32_t, 18> pArray{};
	std::array<std::array<uint32_t, 256>, 4> sBoxes{};
};

} // namespace aion::loginserver::network::ncrypt
