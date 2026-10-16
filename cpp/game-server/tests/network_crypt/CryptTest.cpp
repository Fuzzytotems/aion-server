#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <regex>
#include <span>
#include <string>
#include <vector>

#include "GameClientCrypto.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/EncryptionKeyPair.h"

namespace aion::gameserver::network {
namespace {

using test::GameClientCrypto;

std::vector<uint8_t> bytes(std::initializer_list<int> values) {
	std::vector<uint8_t> result;
	for (int v : values)
		result.push_back(static_cast<uint8_t>(v));
	return result;
}

std::array<uint8_t, 8> keyBytes(std::initializer_list<int> values) {
	std::array<uint8_t, 8> result{};
	size_t i = 0;
	for (int v : values)
		result.at(i++) = static_cast<uint8_t>(v);
	return result;
}

uint64_t keyValue(const std::array<uint8_t, 8>& key) {
	uint64_t value = 0;
	for (int i = 7; i >= 0; i--)
		value = (value << 8) | key[static_cast<size_t>(i)];
	return value;
}

// --- vectors derived by hand from EncryptionKeyPair.java -------------------------------------------------------------------------------------

TEST(EncryptionKeyPairTest, InitialKeysAreTheLittleEndianBaseKeyFollowedByTheConstantHighWord) {
	EncryptionKeyPair pair(0x04030201);
	EXPECT_EQ(pair.getBaseKey(), 0x04030201);
	EXPECT_EQ(pair.getServerKey(), keyBytes({0x01, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87}));
	EXPECT_EQ(pair.getClientKey(), pair.getServerKey());

	EncryptionKeyPair negative(-2);
	EXPECT_EQ(negative.getServerKey(), keyBytes({0xFE, 0xFF, 0xFF, 0xFF, 0xA1, 0x6C, 0x54, 0x87}));
}

TEST(EncryptionKeyPairTest, StaticKeyIsTheJavaAsciiText) {
	const std::string_view text = "nKO/WctQ0AVLbpzfBkS6NevDYT8ourG5CRlmdjyJ72aswx4EPq1UgZhFMXH?3iI9";
	ASSERT_EQ(EncryptionKeyPair::staticKey.size(), text.size());
	for (size_t i = 0; i < text.size(); i++)
		EXPECT_EQ(EncryptionKeyPair::staticKey[i], static_cast<uint8_t>(text[i])) << i;
}

TEST(EncryptionKeyPairTest, EncryptMatchesHandDerivedVector) {
	// key 01 02 03 04 A1 6C 54 87, static key 'n' 'K' 'O' '/' 'W' = 6E 4B 4F 2F 57
	// c0 = 10 ^ 01                = 11
	// c1 = 20 ^ 4B ^ 02 ^ c0(11)  = 78
	// c2 = 30 ^ 4F ^ 03 ^ c1(78)  = 04
	// c3 = 40 ^ 2F ^ 04 ^ c2(04)  = 6F
	// c4 = 50 ^ 57 ^ A1 ^ c3(6F)  = C9     (key[4] = A1)
	// key: 0x87546CA104030201 + 5 = 0x87546CA104030206
	EncryptionKeyPair pair(0x04030201);
	auto data = bytes({0x10, 0x20, 0x30, 0x40, 0x50});
	pair.encrypt(data);
	EXPECT_EQ(data, bytes({0x11, 0x78, 0x04, 0x6F, 0xC9}));
	EXPECT_EQ(pair.getServerKey(), keyBytes({0x06, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87}));
	EXPECT_EQ(pair.getClientKey(), keyBytes({0x01, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87})) << "encrypt must not touch the client key";
}

TEST(EncryptionKeyPairTest, EncryptCarriesIntoTheHighWordAndUsesTheAdvancedKeyForTheNextPacket) {
	// base key -1: FF FF FF FF A1 6C 54 87; + 16 = 0x87546CA20000000F. Bytes >= 0x80 check that Java's signed byte arithmetic has no effect.
	EncryptionKeyPair pair(-1);
	const auto plain = bytes({0x80, 0xFF, 0x00, 0x7F, 0x01, 0xFE, 0xAA, 0x55, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0});
	auto data = plain;
	pair.encrypt(data);
	EXPECT_EQ(data, bytes({0x7F, 0x34, 0x84, 0x2B, 0xDC, 0x2D, 0xA7, 0x24, 0xF9, 0x73, 0x8C, 0x47, 0x1E, 0xBE, 0x4E, 0x5F}));
	EXPECT_EQ(pair.getServerKey(), keyBytes({0x0F, 0x00, 0x00, 0x00, 0xA2, 0x6C, 0x54, 0x87}));

	data = plain;
	pair.encrypt(data);
	EXPECT_EQ(data, bytes({0x8F, 0x3B, 0x74, 0x24, 0xD0, 0x21, 0xAB, 0x28, 0x05, 0x70, 0x70, 0x44, 0x1E, 0xBE, 0x4E, 0x5F}));
	EXPECT_EQ(pair.getServerKey(), keyBytes({0x1F, 0x00, 0x00, 0x00, 0xA2, 0x6C, 0x54, 0x87}));
}

TEST(EncryptionKeyPairTest, DecryptMatchesHandDerivedClientVector) {
	// CM_VERSION_CHECK (opcode 0): wire opcode (((0 + 207) ^ EF) + 0C) ^ EF = C3, frame body C3 00 65 3C FF 01 02
	// client encryption with key 01 02 03 04 A1 6C 54 87:
	// c0 = C3 ^ 01 = C2; c1 = 00 ^ 4B ^ 02 ^ C2 = 8B; c2 = 65 ^ 4F ^ 03 ^ 8B = A2; c3 = 3C ^ 2F ^ 04 ^ A2 = B5; c4 = FF ^ 57 ^ A1 ^ B5 = BC;
	// c5 = 01 ^ 63 ^ 6C ^ BC = B2; c6 = 02 ^ 74 ^ 54 ^ B2 = 90
	EncryptionKeyPair pair(0x04030201);
	auto data = bytes({0xC2, 0x8B, 0xA2, 0xB5, 0xBC, 0xB2, 0x90});
	EXPECT_TRUE(pair.decrypt(data));
	EXPECT_EQ(data, bytes({0xC3, 0x00, 0x65, 0x3C, 0xFF, 0x01, 0x02}));
	EXPECT_EQ(pair.getClientKey(), keyBytes({0x08, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87}));
	EXPECT_EQ(pair.getServerKey(), keyBytes({0x01, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87})) << "decrypt must not touch the server key";
}

TEST(EncryptionKeyPairTest, InvalidClientPacketsDoNotAdvanceTheKey) {
	const auto initialKey = keyBytes({0x01, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87});
	GameClientCrypto client(0x04030201);
	const auto check = [&](std::vector<uint8_t> plain, bool valid) {
		EncryptionKeyPair pair(0x04030201);
		auto data = plain;
		client.encryptClientBodyWithoutAdvancing(data);
		EXPECT_EQ(pair.decrypt(data), valid);
		EXPECT_EQ(data, plain) << "decryption itself does not depend on validity";
		if (valid)
			EXPECT_EQ(keyValue(pair.getClientKey()), keyValue(initialKey) + plain.size());
		else
			EXPECT_EQ(pair.getClientKey(), initialKey);
	};
	check(bytes({0xC3, 0x00, 0x65, 0x3C, 0xFF}), true);              // minimal valid packet
	check(bytes({0xC3, 0x00, 0x65, 0x3C}), false);                   // shorter than 5 bytes
	check(bytes({0xC3, 0x00, 0x44, 0x3C, 0xFF}), false);             // server code instead of 0x65
	check(bytes({0xC3, 0x00, 0x65, 0x3C, 0xFE}), false);             // high byte of the complement wrong
	check(bytes({0xC3, 0x00, 0x65, 0x3D, 0xFF}), false);             // low byte of the complement wrong
	check(bytes({0x00, 0x80, 0x65, 0xFF, 0x7F, 0x99}), true);        // sign bits: short 0x8000 == ~short 0x7FFF
	check(bytes({0x00, 0x00, 0x65, 0xFF, 0xFF}), true);              // 0 == ~(-1)
	check(bytes({0xFF, 0xFF, 0x65, 0x00, 0x00, 0x01, 0x02}), true);  // -1 == ~0
}

TEST(EncryptionKeyPairTest, EmptySpansAreInvalidAndLeaveTheKeysUnchanged) {
	EncryptionKeyPair pair(12345);
	const auto server = pair.getServerKey();
	const auto clientKey = pair.getClientKey();
	EXPECT_FALSE(pair.decrypt({}));
	pair.encrypt({});
	EXPECT_EQ(pair.getServerKey(), server);
	EXPECT_EQ(pair.getClientKey(), clientKey);
}

TEST(EncryptionKeyPairTest, ToStringFormatsKeyBytesWithoutPaddingLikeIntegerToHexString) {
	EncryptionKeyPair pair(0x0A00F001);
	const std::string text = pair.toString();
	// bytes 01 F0 00 0A A1 6C 54 87 -> "1" "f0" "0" "a" "a1" "6c" "54" "87"
	EXPECT_TRUE(std::regex_match(text, std::regex(R"(\{client:0x1f00aa16c5487,server:0x1f00aa16c5487,base:0xa00f001,update:\d+\})"))) << text;
	EXPECT_NE(EncryptionKeyPair(-1).toString().find(",base:0xffffffff,"), std::string::npos);
}

// --- Crypt ----------------------------------------------------------------------------------------------------------------------------------

TEST(CryptTest, EnableKeyReturnsTheEncipheredBaseKey) {
	// (0x04030201 ^ 0xCD92E4DF) + 0x3FF2CCCF = 0xC991E6DE + 0x3FF2CCCF = 0x10984B3AD, int overflow -> 0x0984B3AD
	Crypt crypt;
	EXPECT_EQ(crypt.enableKey(0x04030201), 0x0984B3AD);
	ASSERT_NE(crypt.getPacketKey(), nullptr);
	EXPECT_EQ(crypt.getPacketKey()->getBaseKey(), 0x04030201);

	Crypt other;
	EXPECT_EQ(other.enableKey(-1), static_cast<int32_t>(0x725FE7EFu)); // (0xFFFFFFFF ^ 0xCD92E4DF) + 0x3FF2CCCF = 0x326D1B20 + 0x3FF2CCCF

	for (int32_t key : {0, 1, -1, 0x04030201, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()}) {
		Crypt c;
		EXPECT_EQ(GameClientCrypto::decipherKey(c.enableKey(key)), key);
	}
}

TEST(CryptTest, RandomEnableKeyCanBeDecipheredByTheClient) {
	for (int i = 0; i < 100; i++) {
		Crypt crypt;
		const int32_t sent = crypt.enableKey();
		ASSERT_NE(crypt.getPacketKey(), nullptr);
		EXPECT_EQ(GameClientCrypto::decipherKey(sent), crypt.getPacketKey()->getBaseKey());
	}
}

TEST(CryptTest, EnableKeyTwiceThrows) {
	Crypt crypt;
	crypt.enableKey(1);
	EXPECT_THROW(crypt.enableKey(2), commons::utils::IllegalStateException);
	EXPECT_THROW(crypt.enableKey(), commons::utils::IllegalStateException);
	EXPECT_EQ(crypt.getPacketKey()->getBaseKey(), 1);
}

TEST(CryptTest, FirstEncryptOnlyEnablesTheCrypt) {
	Crypt crypt;
	EXPECT_FALSE(crypt.isEnabled());
	crypt.enableKey(0x04030201);
	EXPECT_FALSE(crypt.isEnabled()) << "enableKey does not enable the crypt, the first encrypt does";
	auto first = bytes({0xC8, 0x01, 0x44, 0x37, 0xFE, 0xAD, 0xB3, 0x84, 0x09});
	const auto unchanged = first;
	crypt.encrypt(first);
	EXPECT_TRUE(crypt.isEnabled());
	EXPECT_EQ(first, unchanged);
	EXPECT_EQ(crypt.getPacketKey()->getServerKey(), keyBytes({0x01, 0x02, 0x03, 0x04, 0xA1, 0x6C, 0x54, 0x87}));

	auto second = bytes({0x10, 0x20, 0x30, 0x40, 0x50});
	crypt.encrypt(second);
	EXPECT_EQ(second, bytes({0x11, 0x78, 0x04, 0x6F, 0xC9}));
}

TEST(CryptTest, EncryptOrDecryptWithoutKeyThrows) {
	Crypt crypt;
	auto data = bytes({1, 2, 3, 4, 5});
	EXPECT_THROW(crypt.decrypt(data), commons::utils::IllegalStateException);
	crypt.encrypt(data); // enables without touching the key, like Java
	EXPECT_TRUE(crypt.isEnabled());
	EXPECT_THROW(crypt.encrypt(data), commons::utils::IllegalStateException);
}

TEST(CryptTest, ServerOpcodeObfuscation) {
	// SM_KEY = 72: (72 + 207) ^ 0xDF = 0x117 ^ 0xDF = 0x1C8
	EXPECT_EQ(Crypt::encodeServerPacketOpcode(72), 0x1C8);
	EXPECT_EQ(Crypt::encodeServerPacketOpcode(0), 0x10); // 0xCF ^ 0xDF
	EXPECT_EQ(Crypt::encodeServerPacketOpcode(303), 0x121); // 510 = 0x1FE ^ 0xDF
	for (int32_t opcode = 0; opcode < 0x10000; opcode++)
		ASSERT_EQ(Crypt::encodeServerPacketOpcode(opcode), (opcode + 207) ^ 0xDF);
	EXPECT_EQ(Crypt::encodeServerPacketOpcode(std::numeric_limits<int32_t>::max()), static_cast<int32_t>((0x7FFFFFFFu + 207u) ^ 0xDFu));
}

TEST(CryptTest, ClientOpcodeObfuscationIsTheInverseOfTheClientEncoding) {
	// CM_VERSION_CHECK = 0: wire C3 -> C3 ^ EF = 2C, - 0C = 20, ^ EF = CF, - 207 = 0
	EXPECT_EQ(Crypt::decodeClientPacketOpcode(0xC3), 0);
	// precedence: (op ^ 0xEF) - 0xC ^ 0xEF is ((op ^ 0xEF) - 0xC) ^ 0xEF in Java, not (op ^ 0xEF) - (0xC ^ 0xEF)
	EXPECT_NE(Crypt::decodeClientPacketOpcode(0xC3), ((0xC3 ^ 0xEF) - (0xC ^ 0xEF)) - 207);
	for (int32_t opcode = 0; opcode < 250; opcode++)
		ASSERT_EQ(Crypt::decodeClientPacketOpcode(GameClientCrypto::clientWireOpcode(opcode)), opcode) << opcode;
	// every 16-bit wire value decodes to a distinct opcode (the factory rejects results outside [0, 250))
	std::vector<int32_t> decoded;
	for (int32_t wire = 0; wire <= 0xFFFF; wire++)
		decoded.push_back(Crypt::decodeClientPacketOpcode(wire));
	std::ranges::sort(decoded);
	EXPECT_EQ(std::ranges::adjacent_find(decoded), decoded.end());
	EXPECT_EQ(Crypt::decodeClientPacketOpcode(0), ((0 ^ 0xEF) - 0xC ^ 0xEF) - 207);
	EXPECT_EQ(Crypt::decodeClientPacketOpcode(0), -195); // 0xEF - 0x0C = 0xE3, ^ 0xEF = 0x0C = 12, - 207
}

// --- round trips against the inverse client implementation -----------------------------------------------------------------------------------

/** Writes a server frame like AionServerPacket.write: length, obfuscated opcode, static code, complement, body; encrypts after the length. */
std::vector<uint8_t> writeServerFrame(Crypt& crypt, int32_t opcode, std::span<const uint8_t> body) {
	std::vector<uint8_t> frame;
	GameClientCrypto::putShort(frame, 0);
	const int32_t op = Crypt::encodeServerPacketOpcode(opcode);
	GameClientCrypto::putShort(frame, static_cast<uint16_t>(op));
	frame.push_back(Crypt::staticServerPacketCode);
	GameClientCrypto::putShort(frame, static_cast<uint16_t>(~op));
	frame.insert(frame.end(), body.begin(), body.end());
	frame[0] = static_cast<uint8_t>(frame.size());
	frame[1] = static_cast<uint8_t>(frame.size() >> 8);
	crypt.encrypt(std::span<uint8_t>(frame).subspan(2));
	return frame;
}

TEST(CryptRoundTripTest, KeyExchangeThenManyPacketsInBothDirections) {
	std::mt19937_64 random(0x5EED);
	for (int32_t baseKeyChoice : {0, 1, 2, 3, 4}) {
		Crypt server;
		// SM_KEY: the first frame, unencrypted, carries the enciphered key
		const int32_t fixedKeys[] = {0, -1, std::numeric_limits<int32_t>::min(), 0x04030201};
		const int32_t sentKey = baseKeyChoice < 4 ? server.enableKey(fixedKeys[baseKeyChoice]) : server.enableKey();
		std::vector<uint8_t> keyBody;
		for (int i = 0; i < 4; i++)
			keyBody.push_back(static_cast<uint8_t>(static_cast<uint32_t>(sentKey) >> (8 * i)));
		auto keyFrame = writeServerFrame(server, 72, keyBody);
		ASSERT_TRUE(server.isEnabled());

		GameClientCrypto client;
		ASSERT_EQ(GameClientCrypto::getShort(keyFrame, 0), keyFrame.size());
		ASSERT_EQ(GameClientCrypto::getShort(keyFrame, 2), GameClientCrypto::serverWireOpcode(72));
		ASSERT_EQ(keyFrame[4], 0x44);
		ASSERT_EQ(GameClientCrypto::getShort(keyFrame, 5), static_cast<uint16_t>(~GameClientCrypto::serverWireOpcode(72)));
		const auto receivedKey = static_cast<int32_t>(keyFrame[7] | (keyFrame[8] << 8) | (keyFrame[9] << 16) | (static_cast<uint32_t>(keyFrame[10]) << 24));
		client.setBaseKey(GameClientCrypto::decipherKey(receivedKey));
		ASSERT_EQ(client.getServerKey(), keyValue(server.getPacketKey()->getServerKey()));

		uint64_t expectedServerKey = client.getServerKey();
		uint64_t expectedClientKey = client.getClientKey();
		for (int i = 0; i < 400; i++) {
			// sizes: mostly small, some around the 8-byte/64-byte key periods, some up to the 8,192-byte client limit
			const size_t bodySize = i % 50 == 0 ? static_cast<size_t>(random() % 8186) : static_cast<size_t>(random() % 130);
			std::vector<uint8_t> body(bodySize);
			for (auto& b : body)
				b = static_cast<uint8_t>(random());

			const auto opcode = static_cast<int32_t>(random() % 304);
			if (random() % 2 == 0) {
				auto frame = writeServerFrame(server, opcode, body);
				expectedServerKey += frame.size() - 2;
				EXPECT_EQ(keyValue(server.getPacketKey()->getServerKey()), expectedServerKey);
				client.decryptServerBody(std::span<uint8_t>(frame).subspan(2));
				ASSERT_EQ(GameClientCrypto::getShort(frame, 2), GameClientCrypto::serverWireOpcode(opcode));
				ASSERT_EQ(frame[4], 0x44);
				ASSERT_EQ(GameClientCrypto::getShort(frame, 5), static_cast<uint16_t>(~GameClientCrypto::getShort(frame, 2)));
				ASSERT_TRUE(std::equal(body.begin(), body.end(), frame.begin() + 7));
			} else {
				const auto clientOpcode = static_cast<int32_t>(random() % 250);
				auto frame = client.buildClientFrame(clientOpcode, body);
				expectedClientKey += frame.size() - 2;
				auto payload = std::span<uint8_t>(frame).subspan(2);
				ASSERT_TRUE(server.decrypt(payload)) << "packet " << i;
				EXPECT_EQ(keyValue(server.getPacketKey()->getClientKey()), expectedClientKey);
				EXPECT_EQ(Crypt::decodeClientPacketOpcode(GameClientCrypto::getShort(payload, 0)), clientOpcode);
				ASSERT_EQ(payload[2], 0x65);
				ASSERT_TRUE(std::equal(body.begin(), body.end(), payload.begin() + 5));
			}
		}
		EXPECT_EQ(client.getServerKey(), keyValue(server.getPacketKey()->getServerKey()));
		EXPECT_EQ(client.getClientKey(), keyValue(server.getPacketKey()->getClientKey()));
	}
}

TEST(CryptRoundTripTest, EverySizeFromOneToThreeHundred) {
	Crypt server;
	server.enableKey(0x12345678);
	std::vector<uint8_t> dummy(1);
	server.encrypt(dummy); // SM_KEY
	GameClientCrypto client(0x12345678);
	for (size_t size = 1; size <= 300; size++) {
		std::vector<uint8_t> plain(size);
		for (size_t i = 0; i < size; i++)
			plain[i] = static_cast<uint8_t>(size * 31 + i * 7);
		auto data = plain;
		server.encrypt(data);
		if (size > 1)
			EXPECT_NE(data, plain) << size;
		client.decryptServerBody(data);
		ASSERT_EQ(data, plain) << size;

		if (size >= 5) {
			auto clientPlain = plain;
			clientPlain[2] = 0x65;
			clientPlain[3] = static_cast<uint8_t>(~clientPlain[0]);
			clientPlain[4] = static_cast<uint8_t>(~clientPlain[1]);
			auto clientData = clientPlain;
			client.encryptClientBody(clientData);
			ASSERT_TRUE(server.decrypt(clientData)) << size;
			ASSERT_EQ(clientData, clientPlain) << size;
		}
	}
	EXPECT_EQ(client.getServerKey(), keyValue(server.getPacketKey()->getServerKey()));
	EXPECT_EQ(client.getClientKey(), keyValue(server.getPacketKey()->getClientKey()));
}

TEST(CryptRoundTripTest, CorruptClientPacketKeepsTheSessionUsable) {
	// AionConnection tolerates up to 2 failed decrypts; the key only advances for valid packets, so the next valid packet still decrypts
	Crypt server;
	server.enableKey(777);
	std::vector<uint8_t> dummy(4);
	server.encrypt(dummy);
	GameClientCrypto client(777);

	auto corrupt = bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
	client.encryptClientBodyWithoutAdvancing(corrupt);
	EXPECT_FALSE(server.decrypt(corrupt));

	const std::vector<uint8_t> data = {9, 8, 7};
	auto frame = client.buildClientFrame(0x17, data);
	auto payload = std::span<uint8_t>(frame).subspan(2);
	ASSERT_TRUE(server.decrypt(payload));
	EXPECT_EQ(Crypt::decodeClientPacketOpcode(GameClientCrypto::getShort(payload, 0)), 0x17);
	EXPECT_EQ(client.getClientKey(), keyValue(server.getPacketKey()->getClientKey()));
}

} // namespace
} // namespace aion::gameserver::network
