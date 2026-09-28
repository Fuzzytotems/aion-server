#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace aion::gameserver::network::test {

/**
 * The CLIENT side of the Aion 4.8 game connection cipher, written from the protocol description instead of the server classes, for tests that
 * talk to the game server like the game client does.
 * <p>
 * <b>Key exchange.</b> The first server frame (SM_KEY, opcode 72) is unencrypted and carries the enciphered key E as its int32 body.
 * The base key is K = (E - 0x3FF2CCCF) ^ 0xCD92E4DF. Both directions start with the 64-bit little endian key K | 0x87546CA1 &lt;&lt; 32.
 * <p>
 * <b>Cipher.</b> For a packet body b (everything after the 2-byte length) with 64-bit key k:
 * <pre>
 * encrypt: c[0] = b[0] ^ k.byte(0);  c[i] = b[i] ^ S[i % 64] ^ k.byte(i % 8) ^ c[i - 1]
 * decrypt: b[0] = c[0] ^ k.byte(0);  b[i] = c[i] ^ S[i % 64] ^ k.byte(i % 8) ^ c[i - 1]
 * </pre>
 * where S is the ASCII text "nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9". After each packet the direction's key grows by
 * the body size (mod 2^64). The server only advances its copy of the client key for packets it accepts, so a test that sends a corrupt packet
 * must not advance clientKey either (sendCorrupt).
 * <p>
 * <b>Opcodes.</b> Server frames: [u16 (opcode + 207) ^ 0xDF][u8 0x44][u16 complement]. Client frames: [u16 wire][u8 0x65][u16 ~wire] with
 * wire = (((opcode + 207) ^ 0xEF) + 0x0C) ^ 0xEF, which the server decodes with ((wire ^ 0xEF) - 0x0C ^ 0xEF) - 207.
 */
class GameClientCrypto {
public:
	static constexpr std::string_view STATIC_KEY = "nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9";
	static constexpr uint64_t KEY_HIGH_WORD = 0x87546CA1ull << 32;
	static constexpr int32_t INTERNAL_VERSION = 207;

	/** Recovers the base key from the int32 that SM_KEY carries */
	static constexpr int32_t decipherKey(int32_t sentKey) noexcept {
		return static_cast<int32_t>((static_cast<uint32_t>(sentKey) - 0x3FF2CCCFu) ^ 0xCD92E4DFu);
	}

	static constexpr uint16_t serverWireOpcode(int32_t opcode) noexcept { return static_cast<uint16_t>((opcode + INTERNAL_VERSION) ^ 0xDF); }

	/** The wire value a client sends for the given (decoded) opcode */
	static constexpr uint16_t clientWireOpcode(int32_t opcode) noexcept {
		return static_cast<uint16_t>((((opcode + INTERNAL_VERSION) ^ 0xEF) + 0x0C) ^ 0xEF);
	}

	GameClientCrypto() = default;
	explicit GameClientCrypto(int32_t baseKey) noexcept { setBaseKey(baseKey); }

	void setBaseKey(int32_t baseKey) noexcept {
		serverKey = KEY_HIGH_WORD | static_cast<uint32_t>(baseKey);
		clientKey = serverKey;
	}

	uint64_t getServerKey() const noexcept { return serverKey; }
	uint64_t getClientKey() const noexcept { return clientKey; }

	/** Decrypts a server packet body in place (the inverse of the server's encrypt) and advances the server key */
	void decryptServerBody(std::span<uint8_t> body) noexcept {
		uint8_t previousCipher = 0;
		for (size_t i = 0; i < body.size(); i++) {
			const uint8_t cipher = body[i];
			uint8_t mask = keyByte(serverKey, i % 8);
			if (i > 0)
				mask ^= static_cast<uint8_t>(STATIC_KEY[i % 64]) ^ previousCipher;
			body[i] = cipher ^ mask;
			previousCipher = cipher;
		}
		serverKey += body.size();
	}

	/** Encrypts a client packet body in place (the inverse of the server's decrypt) and advances the client key */
	void encryptClientBody(std::span<uint8_t> body) noexcept {
		encryptWith(clientKey, body);
		clientKey += body.size();
	}

	/** Encrypts a client packet body without advancing the client key, like a packet the server is expected to reject */
	void encryptClientBodyWithoutAdvancing(std::span<uint8_t> body) const noexcept {
		uint64_t key = clientKey;
		encryptWith(key, body);
	}

	/** Builds a whole client frame [u16 length][wire opcode][0x65][~wire opcode][data] and encrypts everything after the length */
	std::vector<uint8_t> buildClientFrame(int32_t opcode, std::span<const uint8_t> data) {
		const uint16_t wire = clientWireOpcode(opcode);
		std::vector<uint8_t> frame;
		const size_t length = 2 + 5 + data.size();
		if (length > 0xFFFF)
			throw std::invalid_argument("client frame too large");
		frame.reserve(length);
		putShort(frame, static_cast<uint16_t>(length));
		putShort(frame, wire);
		frame.push_back(0x65);
		putShort(frame, static_cast<uint16_t>(~wire));
		frame.insert(frame.end(), data.begin(), data.end());
		encryptClientBody(std::span<uint8_t>(frame).subspan(2));
		return frame;
	}

	static void putShort(std::vector<uint8_t>& out, uint16_t value) {
		out.push_back(static_cast<uint8_t>(value & 0xFF));
		out.push_back(static_cast<uint8_t>(value >> 8));
	}

	static uint16_t getShort(std::span<const uint8_t> data, size_t index) { return static_cast<uint16_t>(data[index] | (data[index + 1] << 8)); }

private:
	static uint8_t keyByte(uint64_t key, size_t index) noexcept { return static_cast<uint8_t>(key >> (8 * index)); }

	static void encryptWith(uint64_t key, std::span<uint8_t> body) noexcept {
		uint8_t previousCipher = 0;
		for (size_t i = 0; i < body.size(); i++) {
			uint8_t mask = keyByte(key, i % 8);
			if (i > 0)
				mask ^= static_cast<uint8_t>(STATIC_KEY[i % 64]) ^ previousCipher;
			body[i] ^= mask;
			previousCipher = body[i];
		}
	}

	uint64_t serverKey = 0;
	uint64_t clientKey = 0;
};

} // namespace aion::gameserver::network::test
