#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/network/ncrypt/CryptEngine.h"
#include "aion/loginserver/network/ncrypt/KeyGen.h"
#include "support/AionLoginClientCrypto.h"

using namespace aion::commons::utils;
using namespace aion::loginserver::network::ncrypt;
using aion::loginserver::test::AionLoginClientCrypto;

namespace {

std::vector<uint8_t> randomBytes(size_t size) {
	std::vector<uint8_t> bytes(size);
	Rnd::nextBytes(bytes);
	return bytes;
}

void appendInt(std::vector<uint8_t>& data, int32_t value) {
	AionLoginClientCrypto::appendWord(data, static_cast<uint32_t>(value));
}

/**
 * The server side of one client connection, emulating the Java LoginConnection / AionServerPacket.write buffer handling around the real
 * CryptEngine and KeyGen: packets are written into a reused write buffer with stale contents.
 */
class ServerConnection {
public:
	ServerConnection() {
		static std::once_flag once;
		std::call_once(once, KeyGen::init);
		encryptedRSAKeyPair = KeyGen::getEncryptedRSAKeyPair();
		blowfishKey = KeyGen::generateBlowfishKey();
		sessionId = Rnd::nextInt();
		cryptEngine.updateKey(blowfishKey);
	}

	/** AionServerPacket.write: [size][opcode + data] with the slice after the size encrypted with length = payload size - 2 */
	std::vector<uint8_t> write(std::span<const uint8_t> payload) {
		writeBuffer[0] = 0;
		writeBuffer[1] = 0;
		std::ranges::copy(payload, writeBuffer.begin() + 2);
		int32_t size = cryptEngine.encrypt(std::span(writeBuffer).subspan(2), static_cast<int32_t>(payload.size()) - 2) + 2;
		writeBuffer[0] = static_cast<uint8_t>(size);
		writeBuffer[1] = static_cast<uint8_t>(size >> 8);
		return {writeBuffer.begin(), writeBuffer.begin() + size};
	}

	/** SM_INIT.writeImpl (payload with opcode) */
	std::vector<uint8_t> buildSM_INIT() const {
		std::vector<uint8_t> payload{0x00};
		appendInt(payload, sessionId);
		appendInt(payload, 0x0000c621);
		auto modulus = encryptedRSAKeyPair->getEncryptedModulus();
		payload.insert(payload.end(), modulus.begin(), modulus.end());
		payload.insert(payload.end(), 16, uint8_t{0});
		payload.insert(payload.end(), blowfishKey.begin(), blowfishKey.end());
		payload.insert(payload.end(), 7, uint8_t{0});
		payload.push_back(0);
		appendInt(payload, 0);
		payload.insert(payload.end(), {0, 0});
		payload.push_back(0);
		appendInt(payload, 0x3FCE09ED);
		appendInt(payload, 0);
		return payload;
	}

	std::vector<uint8_t> writeSM_INIT() {
		lastPayload = buildSM_INIT();
		return write(lastPayload);
	}

	/** Dispatcher.parse + LoginConnection.decrypt: returns the decrypted body, or nullopt for a wrong checksum */
	std::optional<std::vector<uint8_t>> receive(std::span<const uint8_t> frame) {
		size_t size = static_cast<size_t>(frame[0]) | static_cast<size_t>(frame[1]) << 8;
		EXPECT_EQ(size, frame.size());
		std::vector<uint8_t> body(frame.begin() + 2, frame.end());
		if (!cryptEngine.decrypt(body))
			return std::nullopt;
		return body;
	}

	std::shared_ptr<const EncryptedRSAKeyPair> encryptedRSAKeyPair;
	KeyGen::BlowfishKey blowfishKey;
	int32_t sessionId;
	CryptEngine cryptEngine;
	std::vector<uint8_t> writeBuffer = randomBytes(8192 * 2);
	std::vector<uint8_t> lastPayload;
};

struct LoginData {
	std::string username;
	std::string password;
	int32_t otp;
};

/** CM_LOGIN.readImpl + decryptLoginData, transliterated */
std::optional<LoginData> parseCM_LOGIN(const EncryptedRSAKeyPair& pair, std::span<const uint8_t> body, int32_t expectedSessionId) {
	EXPECT_EQ(body[0], 0x00); // opcode
	size_t remaining = body.size() - 1;
	std::span<const uint8_t> encryptedLoginData = body.subspan(1, remaining - 55);
	size_t pos = 1 + encryptedLoginData.size();
	EXPECT_EQ(static_cast<int32_t>(AionLoginClientCrypto::readWord(body, pos)), expectedSessionId);
	pos += 4;
	EXPECT_TRUE(std::all_of(body.begin() + static_cast<ptrdiff_t>(pos), body.begin() + static_cast<ptrdiff_t>(pos + 16), [](uint8_t b) { return b == 0; }));
	pos += 16;
	EXPECT_EQ(std::vector<uint8_t>(body.begin() + static_cast<ptrdiff_t>(pos), body.begin() + static_cast<ptrdiff_t>(pos + 7)),
		(std::vector<uint8_t>{0x20, 0, 0, 0, 0, 0, 0x01}));
	pos += 7 + 16;
	EXPECT_EQ(body.size() - pos, 12u); // readD() x 3

	std::optional<std::vector<uint8_t>> decryptedOpt = pair.decrypt(encryptedLoginData);
	if (!decryptedOpt)
		return std::nullopt;
	std::vector<uint8_t>& decrypted = *decryptedOpt;
	int length = static_cast<int>(encryptedLoginData.size());
	bool isLoginEx = length > 128;
	int contentStartOffset = isLoginEx ? 78 : 94;
	int usernameByteLength = isLoginEx ? 64 : 14;
	int passwordByteLength = isLoginEx ? 32 : 16;
	for (int offset = length - 128; offset >= 0; offset -= 128)
		std::memmove(decrypted.data() + offset, decrypted.data() + offset + contentStartOffset, static_cast<size_t>(length - (offset + contentStartOffset)));
	auto readString = [&](int offset, int maxLength) {
		int stringLength = 0;
		for (int i = offset; stringLength < maxLength && i < length && decrypted[static_cast<size_t>(i)] != 0; i++)
			stringLength++;
		return std::string(decrypted.begin() + offset, decrypted.begin() + offset + stringLength);
	};
	LoginData data;
	data.username = readString(0, usernameByteLength);
	data.password = readString(usernameByteLength, passwordByteLength);
	data.otp = static_cast<int32_t>(AionLoginClientCrypto::readWord(decrypted, static_cast<size_t>(usernameByteLength + passwordByteLength)));
	return data;
}

std::vector<uint8_t> modulusOf(const EVP_PKEY* key) {
	BIGNUM* n = nullptr;
	if (!EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_N, &n))
		throwOpenSslException("no modulus");
	BignumPtr modulus(n);
	std::vector<uint8_t> bytes(static_cast<size_t>(BN_num_bytes(modulus.get())));
	BN_bn2bin(modulus.get(), bytes.data());
	return bytes;
}

std::vector<uint8_t> bytesOf(std::string_view text) {
	return {text.begin(), text.end()};
}

} // namespace

TEST(AionLoginClientCryptoTest, DecryptsSmInit) {
	ServerConnection server;
	AionLoginClientCrypto client;
	std::vector<uint8_t> frame = server.writeSM_INIT();
	ASSERT_EQ(frame.size(), 202u); // payload 192 -> encrypted 200 + size header

	AionLoginClientCrypto::InitPacket init = client.decryptInitPacket(frame);
	EXPECT_TRUE(client.isInitialized());
	EXPECT_EQ(init.sessionId, server.sessionId);
	EXPECT_EQ(client.getSessionId(), server.sessionId);
	EXPECT_EQ(init.protocolRevision, 0xc621);
	EXPECT_TRUE(std::ranges::equal(init.encryptedModulus, server.encryptedRSAKeyPair->getEncryptedModulus()));
	EXPECT_EQ(init.blowfishKey, server.blowfishKey);
	EXPECT_EQ(client.getBlowfishKey(), server.blowfishKey);
	EXPECT_TRUE(std::ranges::equal(client.getModulus(), modulusOf(server.encryptedRSAKeyPair->getRSAKeyPair())));
	ASSERT_EQ(init.body.size(), 200u);
	EXPECT_TRUE(std::equal(server.lastPayload.begin(), server.lastPayload.end(), init.body.begin()));

	EXPECT_THROW(client.decryptInitPacket(frame), IllegalStateException);
}

TEST(AionLoginClientCryptoTest, ServerPacketsAfterInit) {
	{
		ServerConnection server;
		AionLoginClientCrypto client;
		EXPECT_THROW(client.decryptServerPacket(server.write(std::vector<uint8_t>(8))), IllegalStateException);
	}

	ServerConnection server;
	AionLoginClientCrypto client;
	client.decryptInitPacket(server.writeSM_INIT());
	for (size_t size = 1; size <= 80; size++) {
		std::vector<uint8_t> payload = randomBytes(size);
		std::vector<uint8_t> body = client.decryptServerPacket(server.write(payload));
		ASSERT_EQ(body.size() % 8, 0u);
		ASSERT_GT(body.size(), size - 1);
		if (size % 8 == 5) {
			// the checksum (last word) covers the last payload byte
			EXPECT_EQ(body.size(), size + 3);
			EXPECT_TRUE(std::equal(payload.begin(), payload.end() - 1, body.begin()));
		} else {
			EXPECT_TRUE(std::equal(payload.begin(), payload.end(), body.begin())) << size;
		}
	}
}

TEST(AionLoginClientCryptoTest, SmInitWithOverlappingXorKey) {
	// a first packet with payload size % 8 == 1 loses its last byte to the XOR key word, like in Java
	ServerConnection server;
	AionLoginClientCrypto client;
	std::vector<uint8_t> payload = server.buildSM_INIT();
	payload.push_back(0x5A); // 193 bytes
	std::vector<uint8_t> frame = server.write(payload);
	ASSERT_EQ(frame.size(), 2u + 200u);
	AionLoginClientCrypto::InitPacket init = client.decryptInitPacket(frame);
	EXPECT_TRUE(std::equal(payload.begin(), payload.end() - 1, init.body.begin()));
	EXPECT_EQ(init.blowfishKey, server.blowfishKey);
}

TEST(AionLoginClientCryptoTest, ClientPacketsVerifyOnServer) {
	ServerConnection server;
	AionLoginClientCrypto client;
	EXPECT_THROW(client.encryptClientPacket(std::vector<uint8_t>{0x07}), IllegalStateException);
	client.decryptInitPacket(server.writeSM_INIT());

	for (size_t size = 1; size <= 80; size++) {
		std::vector<uint8_t> payload = randomBytes(size);
		std::vector<uint8_t> frame = client.encryptClientPacket(payload);
		size_t bodySize = (size + 3) / 4 * 4 + 8;
		bodySize = (bodySize + 7) / 8 * 8;
		ASSERT_EQ(frame.size(), bodySize + 2);
		std::optional<std::vector<uint8_t>> body = server.receive(frame);
		ASSERT_TRUE(body.has_value()) << size;
		EXPECT_TRUE(std::equal(payload.begin(), payload.end(), body->begin()));
		EXPECT_EQ(AionLoginClientCrypto::readWord(*body, body->size() - 4), 0u);

		// a modified byte breaks the checksum
		std::vector<uint8_t> broken = client.encryptClientPacket(payload);
		broken[2 + static_cast<size_t>(Rnd::nextInt(static_cast<int32_t>(bodySize - 8)))] ^= 0x01;
		EXPECT_FALSE(server.receive(broken).has_value());
	}
}

TEST(AionLoginClientCryptoTest, PadClientPacketLayout) {
	// CM_LOGIN (normal): 172 payload bytes -> 4 zero bytes, checksum, 4 zero bytes
	std::vector<uint8_t> payload(172, 0);
	payload[0] = 0x11;
	payload[171] = 0x22;
	std::vector<uint8_t> body = AionLoginClientCrypto::padClientPacket(payload);
	ASSERT_EQ(body.size(), 184u);
	EXPECT_EQ(AionLoginClientCrypto::readWord(body, 172), 0u);
	EXPECT_EQ(AionLoginClientCrypto::readWord(body, 176), 0x22000011u);
	EXPECT_EQ(AionLoginClientCrypto::readWord(body, 180), 0u);

	EXPECT_EQ(AionLoginClientCrypto::padClientPacket(std::vector<uint8_t>(1)).size(), 16u);
	EXPECT_EQ(AionLoginClientCrypto::padClientPacket(std::vector<uint8_t>(4)).size(), 16u);
	EXPECT_EQ(AionLoginClientCrypto::padClientPacket(std::vector<uint8_t>(5)).size(), 16u);
	EXPECT_EQ(AionLoginClientCrypto::padClientPacket(std::vector<uint8_t>(8)).size(), 16u);
	EXPECT_EQ(AionLoginClientCrypto::padClientPacket(std::vector<uint8_t>(9)).size(), 24u);
}

TEST(AionLoginClientCryptoTest, LoginDataLayoutMatchesJavaDocumentation) {
	// the decrypted examples from the CM_LOGIN documentation
	std::vector<uint8_t> normal = AionLoginClientCrypto::buildLoginData("abcdefghijklmn", "abcdefghijklmnop", -1, false);
	ASSERT_EQ(normal.size(), 128u);
	std::vector<uint8_t> expected(94, 0);
	for (uint8_t b : bytesOf("abcdefghijklmnabcdefghijklmnop"))
		expected.push_back(b);
	expected.insert(expected.end(), 4, uint8_t{0xFF});
	EXPECT_EQ(normal, expected);

	std::vector<uint8_t> loginEx = AionLoginClientCrypto::buildLoginData("abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijklmno3432432pqrs",
		"11111111111111111111111111111111", -1, true);
	ASSERT_EQ(loginEx.size(), 256u);
	expected.assign(78, 0);
	for (uint8_t b : bytesOf("abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijkl"))
		expected.push_back(b);
	expected.insert(expected.end(), 78, uint8_t{0});
	for (uint8_t b : bytesOf("mno3432432pqrs11111111111111111111111111111111"))
		expected.push_back(b);
	expected.insert(expected.end(), 4, uint8_t{0xFF});
	EXPECT_EQ(loginEx, expected);
}

TEST(AionLoginClientCryptoTest, CmLoginEndToEnd) {
	struct Case {
		std::string username;
		std::string password;
		bool loginEx;
		int32_t otp;
		std::string expectedUsername;
		std::string expectedPassword;
	};
	const std::vector<Case> cases = {
		{"admin", "secret", false, -1, "admin", "secret"},
		{"abcdefghijklmn", "abcdefghijklmnop", false, 123456, "abcdefghijklmn", "abcdefghijklmnop"},
		{"abcdefghijklmnopq", "abcdefghijklmnopqrs", false, -1, "abcdefghijklmn", "abcdefghijklmnop"}, // truncated
		{"x", "", false, 0, "x", ""},
		{"admin", "secret", true, -1, "admin", "secret"},
		{"abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijklmno3432432pqrs", "11111111111111111111111111111111", true, 42,
			"abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijklmno3432432pqrs", "11111111111111111111111111111111"},
		{std::string(70, 'u'), std::string(40, 'p'), true, -1, std::string(64, 'u'), std::string(32, 'p')}, // truncated
		{std::string(50, 'u'), "pw", true, 7, std::string(50, 'u'), "pw"}, // username exactly fills the first block
	};

	for (const Case& c : cases) {
		SCOPED_TRACE(c.username + " / " + c.password + (c.loginEx ? " (loginex)" : ""));
		ServerConnection server;
		AionLoginClientCrypto client;
		client.decryptInitPacket(server.writeSM_INIT());
		// a few packets before CM_LOGIN (e.g. CM_AUTH_GG / SM_AUTH_GG)
		ASSERT_TRUE(server.receive(client.encryptClientPacket(randomBytes(47))).has_value());
		client.decryptServerPacket(server.write(randomBytes(11)));

		std::vector<uint8_t> payload = client.buildCM_LOGIN(c.username, c.password, c.loginEx, c.otp);
		ASSERT_EQ(payload.size(), c.loginEx ? 300u : 172u);
		std::optional<std::vector<uint8_t>> body = server.receive(client.encryptClientPacket(payload));
		ASSERT_TRUE(body.has_value());
		std::optional<LoginData> login = parseCM_LOGIN(*server.encryptedRSAKeyPair, *body, server.sessionId);
		ASSERT_TRUE(login.has_value());
		EXPECT_EQ(login->username, c.expectedUsername);
		EXPECT_EQ(login->password, c.expectedPassword);
		EXPECT_EQ(login->otp, c.otp);
	}
}

TEST(AionLoginClientCryptoTest, CmLoginWithWrongKeyPair) {
	ServerConnection server;
	AionLoginClientCrypto client;
	client.decryptInitPacket(server.writeSM_INIT());
	std::vector<uint8_t> payload = client.buildCM_LOGIN("admin", "admin", false, -1, 12345);
	EXPECT_EQ(AionLoginClientCrypto::readWord(payload, 129), 12345u);

	// decrypting with another key pair gives garbage or fails (block >= modulus), but never the credentials
	std::shared_ptr<const EncryptedRSAKeyPair> other = server.encryptedRSAKeyPair;
	while (other == server.encryptedRSAKeyPair)
		other = KeyGen::getEncryptedRSAKeyPair();
	auto decrypted = other->decrypt(std::span(payload).subspan(1, 128));
	if (decrypted)
		EXPECT_NE(*decrypted, AionLoginClientCrypto::buildLoginData("admin", "admin"));
}

TEST(AionLoginClientCryptoTest, MalformedInput) {
	AionLoginClientCrypto client;
	EXPECT_THROW(client.decryptInitPacket(std::vector<uint8_t>{0x01}), IllegalArgumentException);
	EXPECT_THROW(client.decryptInitPacket(std::vector<uint8_t>{0x05, 0x00, 1, 2}), IllegalArgumentException); // size header mismatch
	EXPECT_THROW(client.decryptInitPacket(AionLoginClientCrypto::makeFrame(std::vector<uint8_t>(64))), IllegalArgumentException); // too short
	EXPECT_THROW(client.decryptInitPacket(AionLoginClientCrypto::makeFrame(std::vector<uint8_t>(203))), IllegalArgumentException); // not % 8
	EXPECT_FALSE(client.isInitialized());

	ServerConnection server;
	client.decryptInitPacket(server.writeSM_INIT());
	std::vector<uint8_t> frame = server.write(randomBytes(20));
	frame[5] ^= 0x80;
	EXPECT_THROW(client.decryptServerPacket(frame), IllegalArgumentException);
	EXPECT_THROW(client.decryptServerPacket(AionLoginClientCrypto::makeFrame({})), IllegalArgumentException);

	EXPECT_THROW(AionLoginClientCrypto::decryptModulus(std::vector<uint8_t>(127)), IllegalArgumentException);
	std::vector<uint8_t> modulus(client.getModulus().begin(), client.getModulus().end());
	EXPECT_THROW(AionLoginClientCrypto::rsaEncrypt(modulus, std::vector<uint8_t>(100)), IllegalArgumentException);
	EXPECT_THROW(AionLoginClientCrypto::rsaEncrypt(modulus, modulus), IllegalArgumentException); // not smaller than the modulus
}

TEST(AionLoginClientCryptoTest, MakeFrame) {
	std::vector<uint8_t> frame = AionLoginClientCrypto::makeFrame(std::vector<uint8_t>(300, 7));
	ASSERT_EQ(frame.size(), 302u);
	EXPECT_EQ(frame[0], 0x2E);
	EXPECT_EQ(frame[1], 0x01);
	EXPECT_EQ(AionLoginClientCrypto::frameBody(frame), std::vector<uint8_t>(300, 7));
	EXPECT_THROW(AionLoginClientCrypto::makeFrame(std::vector<uint8_t>(0xFFFE)), IllegalArgumentException);
}

TEST(AionLoginClientCryptoTest, DecXORPassInvertsFirstPacketObfuscationForAllSizes) {
	for (int32_t length = -15; length <= 100; length++) {
		CryptEngine engine;
		engine.updateKey(randomBytes(16));
		std::vector<uint8_t> plain = randomBytes(128);
		std::vector<uint8_t> buffer = plain;
		size_t size = static_cast<size_t>(engine.encrypt(buffer, length));
		std::vector<uint8_t> body(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(size));
		aion::loginserver::network::ncrypt::BlowfishCipher(AionLoginClientCrypto::INITIAL_KEY).decipher(body);
		AionLoginClientCrypto::decXORPass(body);
		size_t keyPos = size == 8 ? 4 : size - 8;
		EXPECT_TRUE(std::equal(body.begin(), body.begin() + static_cast<ptrdiff_t>(keyPos), plain.begin())) << length;
		EXPECT_TRUE(std::equal(body.begin() + static_cast<ptrdiff_t>(keyPos + 4), body.end(), plain.begin() + static_cast<ptrdiff_t>(keyPos + 4))) << length;
	}
}
