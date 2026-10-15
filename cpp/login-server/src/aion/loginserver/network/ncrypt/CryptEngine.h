#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

#include "aion/loginserver/network/ncrypt/BlowfishCipher.h"

namespace aion::loginserver::network::ncrypt {

/**
 * Crypto engine of a client connection: encrypts server packets, decrypts client packets and verifies their checksum.
 * <p>
 * Protocol (all 32-bit words little endian):
 * <ul>
 * <li>The engine starts with a static initial Blowfish key. updateKey() only stores the session key; it takes effect after the first encrypt().</li>
 * <li>The first packet sent (SM_INIT) is padded, obfuscated with encXORPass (a rolling XOR over the words from offset 4 on, seeded with a random
 * key; the final rolling value is stored in the last-but-one word) and encrypted with the initial key. It has no checksum. Then the cipher
 * switches to the session key.</li>
 * <li>Every later packet is padded, gets a checksum in its last word (XOR of all preceding words) and is encrypted with the session key.</li>
 * <li>Received packets are decrypted with the current cipher (the session key, once the first packet was encrypted). The checksum is valid if
 * the length is a multiple of 4 and greater than 4, and the XOR of all words except the last one is 0 (i.e. the client puts its checksum
 * before a final padding word).</li>
 * </ul>
 * <p>
 * Thread safety: all methods lock an internal mutex, so encrypt (write path) and decrypt (read path) may run on different threads. The first
 * encrypt() rebuilds the cipher's key schedule, which must not be read by a concurrent decrypt() (Java shares the cipher between the two paths
 * without synchronization). In the login server both paths already run on the connection's IO strand and updateKey() is called before IO
 * starts, so the mutex is a defensive guarantee of the class itself rather than something LoginConnection relies on; the test
 * DecryptRacingTheFirstEncryptSeesEitherKey verifies it.
 * <p>
 * Java: com.aionemu.loginserver.network.ncrypt.CryptEngine
 *
 * @author EvilSpirit
 */
class CryptEngine {
public:
	/** Upper bound of the bytes encrypt() adds to the length it is given (8 for the checksum/XOR key words plus up to 8 padding bytes) */
	static constexpr int32_t MAX_ENCRYPTION_OVERHEAD = 16;

	/** Initializes the Blowfish cipher with the static initial key used to encrypt the first packet sent to the client. */
	CryptEngine();

	CryptEngine(const CryptEngine&) = delete;
	CryptEngine& operator=(const CryptEngine&) = delete;

	/**
	 * Sets the session key (a copy is stored). The cipher switches to it when the first packet is encrypted; calls after that have no effect,
	 * like in Java.
	 */
	void updateKey(std::span<const uint8_t> newKey);

	/**
	 * Decrypts the packet in place (whole 8-byte blocks; trailing bytes stay unchanged) and verifies its checksum.
	 * <p>
	 * Java: decrypt(data, offset, length) - pass the packet body without its 2-byte length header.
	 * <p>
	 * Deviation: Java compares the checksum loop index with length - 4 instead of offset + length - 4, so for a packet that does not start at
	 * offset 2 of the read buffer (e.g. the second of two packets received in one read) fewer or no words are verified. Offset 2 happens to give
	 * the correct result, and this is what is implemented for every packet.
	 *
	 * @return true if the decrypted packet has a valid checksum
	 */
	bool decrypt(std::span<uint8_t> data);

	/**
	 * Encrypts a packet in place. Java: encrypt(data, offset, length).
	 * <p>
	 * The written size is length + 4 rounded up to the next multiple of 8 (always adding at least one byte), plus 4 more before rounding for
	 * the first packet. The bytes from length up to that size are included in the checksum/XOR pass as they are (Java leaves stale write
	 * buffer contents there), except for the checksum/XOR key word, which is overwritten. Note that this word may overlap the last bytes of
	 * the given length (AionServerPacket passes the payload size minus 2 as length).
	 *
	 * @param data
	 *          buffer starting at the first byte to encrypt (Java: data[offset]), at least as large as the encrypted size
	 *          (max(length + MAX_ENCRYPTION_OVERHEAD, 16) bytes are always enough)
	 * @param length
	 *          number of bytes to encrypt before padding (Java: length). Small negative values are valid like in Java: AionServerPacket passes
	 *          -1 for a packet consisting of only its opcode.
	 * @return size of the encrypted data (a multiple of 8, at least 8)
	 * @throws utils::IllegalArgumentException if the encrypted size would be less than 8 (length &lt; -11, or &lt; -15 for the first packet),
	 * where Java writes outside the encrypted range. Nothing is modified.
	 * @throws utils::IndexOutOfBoundsException if data is smaller than the encrypted size (Java: ArrayIndexOutOfBoundsException). Nothing is
	 * modified in this case.
	 * @throws utils::IllegalStateException if this is the first packet and the session key set with updateKey() is empty. Nothing is modified.
	 */
	int32_t encrypt(std::span<uint8_t> data, int32_t length);

private:
	/** @return true if the XOR of all 32-bit words except the last one is 0 */
	static bool verifyChecksum(std::span<const uint8_t> data) noexcept;

	/** Stores the XOR of all words except the last one in the last word. data.size() must be a multiple of 8. */
	static void appendChecksum(std::span<uint8_t> raw) noexcept;

	/**
	 * First packet obfuscation: rolling XOR over the words [4, size - 8) seeded with key; the final value goes to [size - 8, size - 4), or to
	 * [4, 8) for an 8-byte packet. data.size() must be a multiple of 8, at least 8.
	 */
	static void encXORPass(std::span<uint8_t> data, uint32_t key) noexcept;

	std::mutex mutex;
	/** key to switch to after the first packet (initially the static key) */
	std::vector<uint8_t> key;
	/** true after the first packet was encrypted and the cipher switched to key */
	bool updatedKey = false;
	BlowfishCipher cipher;
};

} // namespace aion::loginserver::network::ncrypt
