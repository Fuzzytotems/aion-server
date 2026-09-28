#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace aion::gameserver::network {

namespace detail {
/** The bytes of an ASCII text of exactly 64 characters */
constexpr std::array<uint8_t, 64> asciiKeyBytes(std::string_view text) noexcept {
	std::array<uint8_t, 64> bytes{};
	for (size_t i = 0; i < bytes.size() && i < text.size(); i++)
		bytes[i] = static_cast<uint8_t>(text[i]);
	return bytes;
}
} // namespace detail

/**
 * The key pair of the game client cipher: one 8-byte key per direction plus a 64-byte static key.
 * <p>
 * Cipher (both directions, applied to everything after the 2-byte length of a frame):
 * <pre>
 * cipher[0] = plain[0] ^ key[0]
 * cipher[i] = plain[i] ^ staticKey[i &amp; 63] ^ key[i &amp; 7] ^ cipher[i - 1]      (i &gt;= 1)
 * </pre>
 * Both keys start as the little endian base key followed by A1 6C 54 87. After each packet the key, read as a little endian 64-bit number,
 * grows by the packet size (with 64-bit wrap-around): always for server packets, and for client packets only if the decrypted packet is valid
 * (at least 5 bytes, byte 2 == 0x65 and the short at 0 equals the complement of the short at 3).
 * <p>
 * Thread safety: none. The owning connection only uses it on its IO strand (decrypt in processData, encrypt in writeData).
 * <p>
 * Java: com.aionemu.gameserver.network.EncryptionKeyPair
 *
 * @author cura
 */
class EncryptionKeyPair {
public:
	/** Initializes client/server encryption keys based on baseKey (a random integer) */
	explicit EncryptionKeyPair(int32_t baseKey);

	/** @return the baseKey used to generate the key pair */
	int32_t getBaseKey() const noexcept { return baseKey; }

	/** Java: toString() - {client:0x...,server:0x...,base:0x...,update:millis}, key bytes as unpadded hex like Integer.toHexString */
	std::string toString() const;

	/**
	 * Decrypts a client packet in place. If the decrypted packet is valid, the client key grows by the packet size.
	 * <p>
	 * Java: decrypt(ByteBuffer) with data = [position, limit) of the buffer. Java validates with absolute indexes, which equal span indexes
	 * because AionConnection always passes a slice starting at position 0 (Dispatcher.parse). An empty span is left untouched (Java would XOR
	 * the byte behind the packet, but the dispatcher never passes an empty packet) and is invalid.
	 *
	 * @return true if decryption was successful
	 */
	bool decrypt(std::span<uint8_t> data) noexcept;

	/** Encrypts a server packet in place (Java: [position, limit) of the buffer) and grows the server key by its size. */
	void encrypt(std::span<uint8_t> data) noexcept;

	/** C++ addition for tests and debugging: the current client key bytes (the key that decrypts the next client packet) */
	const std::array<uint8_t, 8>& getClientKey() const noexcept { return keys[CLIENT]; }

	/** C++ addition for tests and debugging: the current server key bytes (the key that encrypts the next server packet) */
	const std::array<uint8_t, 8>& getServerKey() const noexcept { return keys[SERVER]; }

	/** Java: staticKey - the ASCII bytes of "nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9" */
	static constexpr std::array<uint8_t, 64> staticKey = detail::asciiKeyBytes("nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9");

	/** Second byte of client packet must be equal to this (the byte at index 2 after the length) */
	static constexpr uint8_t staticClientPacketCode = 0x65;

private:
	/** keys index to access SERVER encryption key */
	static constexpr int SERVER = 0;
	/** keys index to access CLIENT encryption key */
	static constexpr int CLIENT = 1;

	/** Check if packet was correctly decoded, also check if packet was correctly coded by aion client */
	static bool validateClientPacket(std::span<const uint8_t> data) noexcept;

	/** Adds size to the key read as a little endian 64-bit number (Java: long arithmetic, wraps) */
	static void addToKey(std::array<uint8_t, 8>& key, uint64_t size) noexcept;

	/** Base key used to generate client/server keys */
	int32_t baseKey = 0;
	/** Encryption keys, indexed by SERVER and CLIENT */
	std::array<std::array<uint8_t, 8>, 2> keys{};
	/** Date of last key use (set once in the constructor, as in Java) */
	int64_t lastUpdate = 0;
};

} // namespace aion::gameserver::network
