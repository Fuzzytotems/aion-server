#pragma once

// FakeGameClient (handlers-and-porting-plan.md §3.1 item 4): the client side of the Aion 4.8 game connection for tests that drive the game server
// like the real game client - key exchange, client packet encryption (the inverse of the server's decrypt), server packet decryption and frame
// validation. Written from the protocol description (network_crypt/GameClientCrypto.h documents the same cipher), not from the server classes.
// C++ only, test support of P4-15 (it belongs in tests/support once the manifest has a TEST_SUPPORT entry for it).

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {

/**
 * Cipher state of the client. The first server frame (SM_KEY, opcode 72) is unencrypted and carries the enciphered key E; the base key is
 * K = (E - 0x3FF2CCCF) ^ 0xCD92E4DF and both directions start with the 64-bit little endian key K | 0x87546CA1 << 32. For a body b with key k:
 * encrypt c[0] = b[0] ^ k.byte(0), c[i] = b[i] ^ S[i % 64] ^ k.byte(i % 8) ^ c[i - 1]; decrypt is the inverse; after each packet the key grows
 * by the body size (the server grows its client key only for packets it accepts).
 */
class FakeGameClientCrypto {
public:
	static constexpr std::string_view STATIC_KEY = "nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9";
	static constexpr uint64_t KEY_HIGH_WORD = 0x87546CA1ull << 32;
	static constexpr int32_t INTERNAL_VERSION = 207;
	static constexpr int32_t SM_KEY_OPCODE = 72;

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

private:
	static uint8_t keyByte(uint64_t key, size_t index) noexcept { return static_cast<uint8_t>(key >> (8 * index)); }

	uint64_t serverKey = 0;
	uint64_t clientKey = 0;
};

/** A blocking fake Aion game client connected to a game server port. Not thread safe. */
class FakeGameClient {
public:
	/** A decrypted and validated server packet */
	struct ServerPacket {
		int32_t opcode = -1;
		/** writeImpl data (after the 5 header bytes) */
		std::vector<uint8_t> data;
	};

	explicit FakeGameClient(uint16_t port) : socket(port) {}

	/**
	 * Reads SM_KEY (the unencrypted first frame) and derives the key. @return the key as sent (enciphered)
	 * @throws std::runtime_error if the first frame is no valid SM_KEY
	 */
	int32_t readKey(std::chrono::milliseconds timeout = 5s) {
		auto frame = socket.readFrame(timeout);
		if (!frame)
			throw std::runtime_error("no SM_KEY received");
		ServerPacket key = parseServerFrame(*frame);
		if (key.opcode != FakeGameClientCrypto::SM_KEY_OPCODE || key.data.size() != 4)
			throw std::runtime_error("the first server packet is no SM_KEY: opcode " + std::to_string(key.opcode));
		const int32_t sentKey = PacketReader(key.data).D();
		crypto.setBaseKey(FakeGameClientCrypto::decipherKey(sentKey));
		keyReceived = true;
		return sentKey;
	}

	/** Builds a client frame [u16 length][wire opcode][0x65][~wire opcode][data], encrypting everything after the length */
	std::vector<uint8_t> buildFrame(int32_t opcode, std::span<const uint8_t> data, bool advanceKey = true) {
		const uint16_t wire = FakeGameClientCrypto::clientWireOpcode(opcode);
		std::vector<uint8_t> frame;
		PacketWriter writer;
		writer.H(static_cast<int32_t>(2 + 5 + data.size())).H(wire).C(0x65).H(static_cast<uint16_t>(~wire)).B(data);
		frame = std::move(writer.data);
		crypto.encryptClientBody(std::span<uint8_t>(frame).subspan(2), advanceKey);
		return frame;
	}

	/** Sends an encrypted client packet with the given (decoded) opcode */
	void sendPacket(int32_t opcode, std::span<const uint8_t> data = {}) { socket.send(buildFrame(opcode, data)); }

	/**
	 * Sends a frame the server cannot validate after decryption (byte 2 is not 0x65) without advancing the client key, like the server that
	 * does not advance its client key for rejected packets.
	 */
	void sendCorruptPacket(size_t size = 16) {
		std::vector<uint8_t> frame(2 + size, uint8_t{0x11});
		frame[0] = static_cast<uint8_t>(frame.size());
		frame[1] = static_cast<uint8_t>(frame.size() >> 8);
		crypto.encryptClientBody(std::span<uint8_t>(frame).subspan(2), false);
		socket.send(frame);
	}

	/** Sends bytes as they are */
	void sendRaw(std::span<const uint8_t> bytes) { socket.send(bytes); }

	/** @return the next server packet (decrypted after SM_KEY), std::nullopt on timeout or close */
	std::optional<ServerPacket> readPacket(std::chrono::milliseconds timeout = 5s) {
		auto frame = socket.readFrame(timeout);
		if (!frame)
			return std::nullopt;
		if (keyReceived)
			crypto.decryptServerBody(std::span<uint8_t>(*frame).subspan(2));
		return parseServerFrame(*frame);
	}

	/** @return the next server packet, failing the test if there is none or it has another opcode */
	ServerPacket expectPacket(int32_t opcode, std::chrono::milliseconds timeout = 5s) {
		std::optional<ServerPacket> packet = readPacket(timeout);
		if (!packet) {
			ADD_FAILURE() << "expected server packet " << opcode << " but got none (closed: " << socket.isClosed() << ")";
			return ServerPacket{};
		}
		EXPECT_EQ(packet->opcode, opcode) << "unexpected server packet";
		return *packet;
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
		if (frame[4] != 0x44 || static_cast<uint16_t>(~wire) != check)
			throw std::runtime_error("invalid server frame header (wrong key?)");
		ServerPacket packet;
		packet.opcode = static_cast<int32_t>((wire ^ 0xDF) - FakeGameClientCrypto::INTERNAL_VERSION) & 0xFFFF;
		packet.data.assign(frame.begin() + 7, frame.end());
		return packet;
	}

	TestSocket socket;
	FakeGameClientCrypto crypto;
	bool keyReceived = false;
};

} // namespace aion::gameserver::network::test
