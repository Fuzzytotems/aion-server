#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aion::clientprotocol {

/** A decrypted and validated server packet of the game connection */
struct ServerPacket {
	int32_t opcode = -1;
	/** writeImpl data (after the 5 header bytes) */
	std::vector<uint8_t> data;
};

/**
 * The client side of the Aion 4.8 game connection: the cipher state, the opcode obfuscation and the frames. Written from the protocol
 * description (network_crypt/GameClientCrypto.h of the game server tests documents the same cipher), not from the server classes.
 * <p>
 * The first server frame (SM_KEY, opcode 72) is unencrypted and carries the enciphered key E; the base key is K = (E - 0x3FF2CCCF) ^ 0xCD92E4DF
 * and both directions start with the 64-bit little endian key K | 0x87546CA1 << 32. For a body b with key k: encrypt c[0] = b[0] ^ k.byte(0),
 * c[i] = b[i] ^ S[i % 64] ^ k.byte(i % 8) ^ c[i - 1]; decrypt is the inverse; after each packet the key grows by the body size (the server grows
 * its client key only for packets it accepts).
 * <p>
 * Frames are [uint16 size, little endian, including itself][body]; splitting the TCP stream into frames is up to the caller. A client body is
 * [wire opcode][0x65][~wire opcode][data], a server body [wire opcode][0x44][~wire opcode][data].
 * <p>
 * Moved from cpp/game-server/tests/support/FakeGameClient.h (FakeGameClientCrypto, FakeGameClient::buildFrame and parseServerFrame). Not
 * thread-safe: the stream cipher state of each direction must stay in order.
 */
class GameClientCrypto {
public:
	static constexpr std::string_view STATIC_KEY = "nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9";
	static constexpr uint64_t KEY_HIGH_WORD = 0x87546CA1ull << 32;
	static constexpr int32_t INTERNAL_VERSION = 207;
	static constexpr int32_t SM_KEY_OPCODE = 72;
	/** the static byte after the wire opcode: client packets 0x65, server packets 0x44 */
	static constexpr uint8_t CLIENT_PACKET_CODE = 0x65;
	static constexpr uint8_t SERVER_PACKET_CODE = 0x44;

	/** Recovers the base key from the int32 that SM_KEY carries */
	static constexpr int32_t decipherKey(int32_t sentKey) noexcept {
		return static_cast<int32_t>((static_cast<uint32_t>(sentKey) - 0x3FF2CCCFu) ^ 0xCD92E4DFu);
	}

	/** The obfuscated opcode of a server frame: (opcode + 207) ^ 0xDF */
	static constexpr uint16_t serverWireOpcode(int32_t opcode) noexcept { return static_cast<uint16_t>((opcode + INTERNAL_VERSION) ^ 0xDF); }

	/** The wire value a client sends for the given (decoded) opcode */
	static constexpr uint16_t clientWireOpcode(int32_t opcode) noexcept {
		return static_cast<uint16_t>((((opcode + INTERNAL_VERSION) ^ 0xEF) + 0x0C) ^ 0xEF);
	}

	void setBaseKey(int32_t baseKey) noexcept {
		serverKey = KEY_HIGH_WORD | static_cast<uint32_t>(baseKey);
		clientKey = serverKey;
	}

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

	void encryptClientBody(std::span<uint8_t> body, bool advanceKey) noexcept {
		uint8_t previousCipher = 0;
		for (size_t i = 0; i < body.size(); i++) {
			uint8_t mask = keyByte(clientKey, i % 8);
			if (i > 0)
				mask ^= static_cast<uint8_t>(STATIC_KEY[i % 64]) ^ previousCipher;
			body[i] ^= mask;
			previousCipher = body[i];
		}
		if (advanceKey)
			clientKey += body.size();
	}

	/**
	 * Builds a client frame [u16 length][wire opcode][0x65][~wire opcode][data], encrypting everything after the length.
	 * @param advanceKey false sends a packet the server is expected to reject (it does not grow its key for those either)
	 * @throws std::invalid_argument if the frame does not fit its 16-bit length
	 */
	std::vector<uint8_t> buildClientFrame(int32_t opcode, std::span<const uint8_t> data, bool advanceKey = true) {
		if (data.size() > 0xFFFF - 7)
			throw std::invalid_argument("client packet too large: " + std::to_string(data.size()));
		const uint16_t wire = clientWireOpcode(opcode);
		const uint16_t check = static_cast<uint16_t>(~wire);
		const size_t size = 7 + data.size();
		std::vector<uint8_t> frame;
		frame.reserve(size);
		frame.push_back(static_cast<uint8_t>(size));
		frame.push_back(static_cast<uint8_t>(size >> 8));
		frame.push_back(static_cast<uint8_t>(wire));
		frame.push_back(static_cast<uint8_t>(wire >> 8));
		frame.push_back(CLIENT_PACKET_CODE);
		frame.push_back(static_cast<uint8_t>(check));
		frame.push_back(static_cast<uint8_t>(check >> 8));
		frame.insert(frame.end(), data.begin(), data.end());
		encryptClientBody(std::span<uint8_t>(frame).subspan(2), advanceKey);
		return frame;
	}

	/**
	 * Validates the 5 header bytes of a (decrypted) server frame: the obfuscated opcode, the static server packet code 0x44 and the complement of
	 * the obfuscated opcode. @throws std::runtime_error if they are inconsistent
	 */
	static ServerPacket parseServerFrame(std::span<const uint8_t> frame) {
		if (frame.size() < 7)
			throw std::runtime_error("server frame too short: " + std::to_string(frame.size()));
		const uint16_t wire = static_cast<uint16_t>(frame[2] | frame[3] << 8);
		const uint16_t check = static_cast<uint16_t>(frame[5] | frame[6] << 8);
		if (frame[4] != SERVER_PACKET_CODE || static_cast<uint16_t>(~wire) != check)
			throw std::runtime_error("invalid server frame header (wrong key?)");
		ServerPacket packet;
		packet.opcode = static_cast<int32_t>((wire ^ 0xDF) - INTERNAL_VERSION) & 0xFFFF;
		packet.data.assign(frame.begin() + 7, frame.end());
		return packet;
	}

private:
	static uint8_t keyByte(uint64_t key, size_t index) noexcept { return static_cast<uint8_t>(key >> (8 * index)); }

	uint64_t serverKey = 0;
	uint64_t clientKey = 0;
};

} // namespace aion::clientprotocol
