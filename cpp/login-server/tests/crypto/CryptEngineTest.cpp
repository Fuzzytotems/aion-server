#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/network/ncrypt/BlowfishCipher.h"
#include "aion/loginserver/network/ncrypt/CryptEngine.h"

using namespace aion::commons::utils;
using aion::loginserver::network::ncrypt::BlowfishCipher;
using aion::loginserver::network::ncrypt::CryptEngine;

namespace {

constexpr std::array<uint8_t, 16> INITIAL_KEY = {0x6b, 0x60, 0xcb, 0x5b, 0x82, 0xce, 0x90, 0xb1, 0xcc, 0x2b, 0x6c, 0x55, 0x6c, 0x6c, 0x6c, 0x6c};

std::vector<uint8_t> randomBytes(size_t size) {
	std::vector<uint8_t> bytes(size);
	Rnd::nextBytes(bytes);
	return bytes;
}

uint32_t readWord(std::span<const uint8_t> data, size_t pos) {
	return static_cast<uint32_t>(data[pos]) | static_cast<uint32_t>(data[pos + 1]) << 8 | static_cast<uint32_t>(data[pos + 2]) << 16 |
		static_cast<uint32_t>(data[pos + 3]) << 24;
}

void writeWord(std::span<uint8_t> data, size_t pos, uint32_t value) {
	for (int i = 0; i < 4; i++)
		data[pos + static_cast<size_t>(i)] = static_cast<uint8_t>(value >> (8 * i));
}

/**
 * Literal transliteration of the Java CryptEngine of upstream bce89b2fc (ByteBuffer by absolute index: the offset is honoured everywhere and the
 * padding is zeroed). The XOR key of the first packet is a parameter like Java's package-private encrypt(data, offset, length, xorKey).
 */
class JavaCryptEngine {
public:
	JavaCryptEngine() : key(INITIAL_KEY.begin(), INITIAL_KEY.end()), cipher(key) {}

	void updateKey(std::vector<uint8_t> newKey) { key = std::move(newKey); }

	bool decrypt(std::vector<uint8_t>& data, int offset, int length) {
		cipher.decipher(std::span(data).subspan(static_cast<size_t>(offset), static_cast<size_t>(length)));
		return verifyChecksum(data, offset, length);
	}

	int encrypt(std::vector<uint8_t>& data, int offset, int length, int32_t xorKey) {
		int dataLength = length;
		length += 4;
		if (!updatedKey) {
			length += 4;
			length += 8 - length % 8;
			clear(data, offset + dataLength, length - dataLength);
			encXORPass(data, offset, length, xorKey);
			cipher.cipher(std::span(data).subspan(static_cast<size_t>(offset), static_cast<size_t>(length)));
			cipher.updateKey(key);
			updatedKey = true;
		} else {
			length += 8 - length % 8;
			clear(data, offset + dataLength, length - dataLength);
			appendChecksum(data, offset, length);
			cipher.cipher(std::span(data).subspan(static_cast<size_t>(offset), static_cast<size_t>(length)));
		}
		return length;
	}

	static void clear(std::vector<uint8_t>& data, int from, int count) {
		for (int i = 0; i < count; i++)
			data[static_cast<size_t>(from + i)] = 0;
	}

	static bool verifyChecksum(const std::vector<uint8_t>& data, int offset, int length) {
		if ((length & 3) != 0 || (length <= 4))
			return false;
		uint32_t chksum = 0;
		for (int i = 0; i < length - 4; i += 4)
			chksum ^= readWord(data, static_cast<size_t>(offset + i));
		return 0 == chksum;
	}

	static void appendChecksum(std::vector<uint8_t>& raw, int offset, int length) {
		uint32_t chksum = 0;
		int count = length - 4;
		for (int i = 0; i < count; i += 4)
			chksum ^= readWord(raw, static_cast<size_t>(offset + i));
		writeWord(raw, static_cast<size_t>(offset + count), chksum);
	}

	static void encXORPass(std::vector<uint8_t>& data, int offset, int length, int32_t key) {
		int stop = offset + length - 8;
		size_t pos = static_cast<size_t>(4 + offset);
		uint32_t ecx = static_cast<uint32_t>(key); // int arithmetic wraps in Java
		while (static_cast<int>(pos) < stop) {
			uint32_t edx = readWord(data, pos);
			ecx += edx;
			edx ^= ecx;
			writeWord(data, pos, edx);
			pos += 4;
		}
		writeWord(data, pos, ecx);
	}

private:
	std::vector<uint8_t> key;
	bool updatedKey = false;
	BlowfishCipher cipher;
};

/** Undoes encXORPass on a deciphered first packet and returns the random initial XOR key */
uint32_t recoverXorKey(std::vector<uint8_t> body) {
	size_t keyPos = std::max<size_t>(body.size(), 12) - 8; // an 8-byte packet has its key at 4
	uint32_t ecx = readWord(body, keyPos);
	for (size_t pos = keyPos - 4; pos >= 4; pos -= 4)
		ecx -= readWord(body, pos) ^ ecx;
	return ecx;
}

int32_t expectedFirstLength(int32_t length) {
	return (length + 8) / 8 * 8 + 8;
}

int32_t expectedLength(int32_t length) {
	return (length + 4) / 8 * 8 + 8;
}

} // namespace

TEST(CryptEngineTest, PaddingArithmetic) {
	// hand-derived examples: the first packet reserves 8 bytes, later packets 4, and padding always adds 1-8 bytes
	EXPECT_EQ(expectedFirstLength(0), 16);
	EXPECT_EQ(expectedFirstLength(7), 16);
	EXPECT_EQ(expectedFirstLength(8), 24);
	EXPECT_EQ(expectedFirstLength(190), 200); // SM_INIT: payload 192 bytes, AionServerPacket passes 190
	EXPECT_EQ(expectedLength(0), 8);
	EXPECT_EQ(expectedLength(3), 8);
	EXPECT_EQ(expectedLength(4), 16);
	EXPECT_EQ(expectedLength(12), 24);
	EXPECT_EQ(expectedLength(-1), 8); // a packet of only an opcode (AionServerPacket passes payload size - 2)
	EXPECT_EQ(expectedFirstLength(-1), 8);

	for (int32_t length = -11; length <= 64; length++) {
		std::vector<uint8_t> buffer(128);
		CryptEngine engine;
		engine.updateKey(randomBytes(16));
		EXPECT_EQ(engine.encrypt(buffer, length), expectedFirstLength(length)) << length;
		EXPECT_EQ(engine.encrypt(buffer, length), expectedLength(length)) << length;
		EXPECT_EQ(engine.encrypt(buffer, length), expectedLength(length)) << length;
		EXPECT_LE(expectedFirstLength(length), std::max(length + CryptEngine::MAX_ENCRYPTION_OVERHEAD, 16));
	}
}

TEST(CryptEngineTest, EncryptMatchesJavaReferenceAtBufferOffset2) {
	// AionServerPacket.write encrypts the packet at offset 2 of the write buffer, whose bytes beyond the packet are stale
	for (int iteration = 0; iteration < 50; iteration++) {
		std::vector<uint8_t> sessionKey = randomBytes(16);
		CryptEngine engine;
		engine.updateKey(sessionKey);
		JavaCryptEngine java;
		java.updateKey(sessionKey);

		for (int packet = 0; packet < 5; packet++) {
			int32_t length = Rnd::get(0, 300); // Java zeroes from offset + length on, which needs a length of at least -offset
			std::vector<uint8_t> buffer = randomBytes(400);
			std::vector<uint8_t> javaBuffer = buffer;

			int32_t size = engine.encrypt(std::span(buffer).subspan(2), length);
			int32_t xorKey = 0;
			if (packet == 0) {
				std::vector<uint8_t> body(buffer.begin() + 2, buffer.begin() + 2 + size);
				BlowfishCipher(INITIAL_KEY).decipher(body);
				xorKey = static_cast<int32_t>(recoverXorKey(body));
			}
			int32_t javaSize = java.encrypt(javaBuffer, 2, length, xorKey);
			ASSERT_EQ(size, javaSize);
			ASSERT_EQ(buffer, javaBuffer) << "packet " << packet << ", length " << length;
		}
	}
}

TEST(CryptEngineTest, FirstPacketFormat) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	CryptEngine engine;
	engine.updateKey(sessionKey);

	std::vector<uint8_t> plain = randomBytes(190);
	std::vector<uint8_t> buffer = plain;
	buffer.resize(256);
	ASSERT_EQ(engine.encrypt(buffer, 190), 200);

	// encrypted with the static initial key
	std::vector<uint8_t> body(buffer.begin(), buffer.begin() + 200);
	BlowfishCipher(INITIAL_KEY).decipher(body);
	// the first word is not obfuscated
	EXPECT_TRUE(std::equal(body.begin(), body.begin() + 4, plain.begin()));
	// undo the rolling XOR: word = stored ^ ecx, ecx -= word (backwards from the word before the key at size - 8)
	uint32_t ecx = readWord(body, 192);
	for (size_t pos = 188; pos >= 4; pos -= 4) {
		uint32_t word = readWord(body, pos) ^ ecx;
		ecx -= word;
		writeWord(body, pos, word);
	}
	EXPECT_TRUE(std::equal(body.begin(), body.begin() + 190, plain.begin()));
	// bytes 190-191 were zero in the buffer (resize), 196-199 too: not touched besides encryption
	EXPECT_EQ(body[190], 0);
	EXPECT_EQ(body[191], 0);
	EXPECT_EQ(readWord(body, 196), 0u);

	// the next packet uses the session key
	std::vector<uint8_t> next(16);
	ASSERT_EQ(engine.encrypt(next, 4), 16);
	BlowfishCipher(sessionKey).decipher(next);
	uint32_t chksum = readWord(next, 0) ^ readWord(next, 4) ^ readWord(next, 8);
	EXPECT_EQ(readWord(next, 12), chksum);
}

TEST(CryptEngineTest, HandDerivedChecksum) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	CryptEngine engine;
	engine.updateKey(sessionKey);
	std::vector<uint8_t> first(64);
	engine.encrypt(first, 0);

	std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x10, 0x20, 0x30, 0x40, 0xAA, 0xBB, 0xCC, 0xDD, 0x55, 0x55, 0x55, 0x55, 0x77};
	ASSERT_EQ(engine.encrypt(data, 4), 16);
	EXPECT_EQ(data[16], 0x77); // beyond the encrypted size
	BlowfishCipher(sessionKey).decipher(std::span(data).first(16));
	// upstream bce89b2fc: the padding is zeroed, so the checksum is the first word: 0x04030201, stored over the stale bytes 55 55 55 55
	EXPECT_EQ(std::vector<uint8_t>(data.begin(), data.begin() + 16),
		(std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0x01, 0x02, 0x03, 0x04}));

	// length 3: the padding (byte 3) is zeroed before the checksum word at 4 is written
	std::vector<uint8_t> small = {0x01, 0x11, 0x22, 0x33, 0x44, 0x00, 0x00, 0x00};
	ASSERT_EQ(engine.encrypt(small, 3), 8);
	BlowfishCipher(sessionKey).decipher(small);
	EXPECT_EQ(small, (std::vector<uint8_t>{0x01, 0x11, 0x22, 0x00, 0x01, 0x11, 0x22, 0x00}));
}

TEST(CryptEngineTest, VerifyChecksumRules) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	BlowfishCipher sessionCipher(sessionKey);
	CryptEngine engine;
	engine.updateKey(sessionKey);
	std::vector<uint8_t> first(64);
	engine.encrypt(first, 0);

	auto decrypts = [&](std::vector<uint8_t> plain) {
		sessionCipher.cipher(plain);
		return engine.decrypt(plain);
	};

	// XOR of all words except the last must be 0; the last word is ignored
	EXPECT_TRUE(decrypts({0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78}));
	EXPECT_FALSE(decrypts({1, 0, 0, 0, 0x12, 0x34, 0x56, 0x78}));
	EXPECT_TRUE(decrypts({0x01, 0x02, 0x03, 0x84, 0x10, 0x20, 0x30, 0x40, 0x11, 0x22, 0x33, 0xC4, 0xFF, 0xFF, 0xFF, 0xFF}));
	EXPECT_FALSE(decrypts({0x01, 0x02, 0x03, 0x84, 0x10, 0x20, 0x30, 0x40, 0x11, 0x22, 0x33, 0x44, 0xFF, 0xFF, 0xFF, 0xFF}));
	// a checksum in the last word, as the server appends it, does not verify (unless it is 0)
	EXPECT_FALSE(decrypts({0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}));
	// the length must be a multiple of 4 and greater than 4
	EXPECT_FALSE(decrypts({0, 0, 0, 0}));
	EXPECT_FALSE(decrypts({}));
	EXPECT_FALSE(decrypts({0, 0, 0, 0, 0, 0, 0, 0, 0}));
	// length 12: only the first block is deciphered, the checksum covers words 0 and 1 (the third, not deciphered, word is ignored)
	std::vector<uint8_t> twelve = {0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04};
	sessionCipher.cipher(twelve);
	twelve.insert(twelve.end(), {0xDE, 0xAD, 0xBE, 0xEF});
	EXPECT_TRUE(engine.decrypt(twelve));
}

TEST(CryptEngineTest, DecryptMatchesJavaReferenceAtBufferOffset2) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	CryptEngine engine;
	engine.updateKey(sessionKey);
	JavaCryptEngine java;
	java.updateKey(sessionKey);
	std::vector<uint8_t> first(64);
	int32_t xorKey = 0;
	{
		engine.encrypt(first, 0);
		std::vector<uint8_t> body(first.begin(), first.begin() + 16);
		BlowfishCipher(INITIAL_KEY).decipher(body);
		xorKey = static_cast<int32_t>(recoverXorKey(body));
		std::vector<uint8_t> javaFirst(64);
		java.encrypt(javaFirst, 0, 0, xorKey);
		ASSERT_EQ(first, javaFirst);
	}

	BlowfishCipher sessionCipher(sessionKey);
	int valid = 0;
	for (int iteration = 0; iteration < 2000; iteration++) {
		int32_t length = Rnd::get(0, 40);
		std::vector<uint8_t> plain = randomBytes(static_cast<size_t>(length));
		// make about half of the packets valid
		if (length >= 8 && length % 4 == 0 && Rnd::nextBoolean()) {
			uint32_t chksum = 0;
			for (int32_t i = 0; i < length - 8; i += 4)
				chksum ^= readWord(plain, static_cast<size_t>(i));
			writeWord(plain, static_cast<size_t>(length - 8), chksum);
		}
		sessionCipher.cipher(plain);

		std::vector<uint8_t> buffer(2);
		buffer.insert(buffer.end(), plain.begin(), plain.end());
		buffer.resize(buffer.size() + 8);
		std::vector<uint8_t> javaBuffer = buffer;

		bool result = engine.decrypt(std::span(buffer).subspan(2, static_cast<size_t>(length)));
		ASSERT_EQ(result, java.decrypt(javaBuffer, 2, length)) << length;
		ASSERT_EQ(buffer, javaBuffer);
		if (result)
			valid++;
	}
	EXPECT_GT(valid, 100);
}

TEST(CryptEngineTest, TheChecksumDoesNotDependOnThePositionInTheBuffer) {
	// upstream bce89b2fc fixed Java's loop bound that ignored the offset (a second packet in the read buffer was not verified at all), which the C++
	// port never had
	std::vector<uint8_t> plain = {0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	std::vector<uint8_t> buffer(20);
	buffer.insert(buffer.end(), plain.begin(), plain.end());
	EXPECT_FALSE(JavaCryptEngine::verifyChecksum(buffer, 20, 16));
	EXPECT_FALSE(JavaCryptEngine::verifyChecksum(plain, 0, 16));

	CryptEngine engine;
	std::vector<uint8_t> encrypted = plain;
	BlowfishCipher(INITIAL_KEY).cipher(encrypted);
	EXPECT_FALSE(engine.decrypt(encrypted));
}

TEST(CryptEngineTest, DecryptUsesInitialKeyUntilFirstPacketWasEncrypted) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	CryptEngine engine;
	engine.updateKey(sessionKey);

	std::vector<uint8_t> plain = {0x07, 0, 0, 0, 0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	std::vector<uint8_t> data = plain;
	BlowfishCipher(INITIAL_KEY).cipher(data);
	EXPECT_TRUE(engine.decrypt(data));
	EXPECT_EQ(data, plain);

	std::vector<uint8_t> first(64);
	engine.encrypt(first, 10);

	data = plain;
	BlowfishCipher(INITIAL_KEY).cipher(data);
	EXPECT_FALSE(engine.decrypt(data) && data == plain);
	data = plain;
	BlowfishCipher(sessionKey).cipher(data);
	EXPECT_TRUE(engine.decrypt(data));
	EXPECT_EQ(data, plain);
}

TEST(CryptEngineTest, UpdateKeyAfterFirstPacketHasNoEffect) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	CryptEngine engine;
	engine.updateKey(randomBytes(16));
	engine.updateKey(sessionKey); // the last key before the first packet wins
	std::vector<uint8_t> first(64);
	engine.encrypt(first, 0);
	engine.updateKey(randomBytes(16));

	std::vector<uint8_t> data(16);
	ASSERT_EQ(engine.encrypt(data, 8), 16);
	BlowfishCipher(sessionKey).decipher(data);
	EXPECT_EQ(readWord(data, 12), 0u); // all zero data -> zero checksum
	EXPECT_EQ(data, std::vector<uint8_t>(16));
}

TEST(CryptEngineTest, InvalidEncryptArguments) {
	CryptEngine engine;
	engine.updateKey(randomBytes(16));

	std::vector<uint8_t> small = randomBytes(15);
	std::vector<uint8_t> copy = small;
	EXPECT_THROW(engine.encrypt(small, 0), IndexOutOfBoundsException); // the first packet needs 16 bytes
	EXPECT_EQ(small, copy);
	EXPECT_THROW(engine.encrypt(small, -16), IllegalArgumentException); // Java would write outside the encrypted range
	EXPECT_EQ(small, copy);

	std::vector<uint8_t> exact(16);
	EXPECT_EQ(engine.encrypt(exact, 0), 16); // still the first packet

	EXPECT_THROW(engine.encrypt(exact, -12), IllegalArgumentException);
	EXPECT_EQ(engine.encrypt(exact, -11), 8); // C++ only: Java zeroes from length on, so a negative length writes before the packet

	std::vector<uint8_t> seven(7);
	EXPECT_THROW(engine.encrypt(seven, 0), IndexOutOfBoundsException);

	CryptEngine emptyKey;
	emptyKey.updateKey({});
	std::vector<uint8_t> buffer = randomBytes(32);
	copy = buffer;
	EXPECT_THROW(emptyKey.encrypt(buffer, 0), IllegalStateException);
	EXPECT_EQ(buffer, copy);
}

// Encrypt and decrypt on different threads after the key switch (the cipher is only read then).
TEST(CryptEngineTest, ConcurrentEncryptAndDecrypt) {
	std::vector<uint8_t> sessionKey = randomBytes(16);
	CryptEngine engine;
	engine.updateKey(sessionKey);

	// valid client packets encrypted with the session key
	std::vector<std::vector<uint8_t>> clientPackets;
	for (int i = 0; i < 200; i++) {
		std::vector<uint8_t> plain = randomBytes(32);
		uint32_t chksum = readWord(plain, 0) ^ readWord(plain, 4) ^ readWord(plain, 8) ^ readWord(plain, 12) ^ readWord(plain, 16) ^ readWord(plain, 20);
		writeWord(plain, 24, chksum);
		BlowfishCipher(sessionKey).cipher(plain);
		clientPackets.push_back(std::move(plain));
	}

	std::vector<uint8_t> first(64);
	engine.encrypt(first, 20);

	std::atomic<int> failures = 0;
	std::thread writer([&] {
		BlowfishCipher check(sessionKey);
		for (int i = 0; i < 2000; i++) {
			std::vector<uint8_t> packet = randomBytes(64);
			int32_t size = engine.encrypt(packet, 20);
			check.decipher(std::span(packet).first(static_cast<size_t>(size)));
			uint32_t chksum = 0;
			for (int32_t p = 0; p < size; p += 4)
				chksum ^= readWord(packet, static_cast<size_t>(p));
			if (chksum != 0)
				failures++;
		}
	});
	std::thread reader([&] {
		for (int i = 0; i < 2000; i++) {
			std::vector<uint8_t> packet = clientPackets[static_cast<size_t>(i) % clientPackets.size()];
			if (!engine.decrypt(packet))
				failures++;
		}
	});
	writer.join();
	reader.join();
	EXPECT_EQ(failures.load(), 0);
}

// The first encrypt() rebuilds the Blowfish key schedule (cipher.updateKey). A decrypt() racing it must see either the initial or the session
// key, never a partially rebuilt P-array/S-boxes (without the engine's mutex this fails: the reader gets results matching neither key).
TEST(CryptEngineTest, DecryptRacingTheFirstEncryptSeesEitherKey) {
	const std::vector<uint8_t> sessionKey = randomBytes(16);
	const std::vector<uint8_t> encrypted = randomBytes(64);
	std::vector<uint8_t> withInitialKey = encrypted;
	BlowfishCipher(INITIAL_KEY).decipher(withInitialKey);
	std::vector<uint8_t> withSessionKey = encrypted;
	BlowfishCipher(sessionKey).decipher(withSessionKey);

	int garbage = 0;
	int overlapping = 0; // iterations in which the reader saw both keys, i.e. really raced the key switch
	for (int iteration = 0; iteration < 300; iteration++) {
		CryptEngine engine;
		engine.updateKey(sessionKey);
		std::atomic<bool> started = false;
		std::atomic<bool> switched = false;
		int readerGarbage = 0;
		bool sawInitial = false, sawSession = false;
		std::thread reader([&] {
			started = true;
			for (int afterSwitch = 0; afterSwitch < 3;) {
				if (switched)
					afterSwitch++;
				std::vector<uint8_t> packet = encrypted;
				engine.decrypt(packet);
				if (packet == withInitialKey)
					sawInitial = true;
				else if (packet == withSessionKey)
					sawSession = true;
				else
					readerGarbage++;
			}
		});
		while (!started)
			std::this_thread::yield();
		std::vector<uint8_t> first(64);
		engine.encrypt(first, 20);
		switched = true;
		reader.join();
		garbage += readerGarbage;
		if (sawInitial && sawSession)
			overlapping++;
	}
	EXPECT_EQ(garbage, 0);
	EXPECT_GT(overlapping, 0) << "the reader never raced the key switch";
}

// upstream bce89b2fc (Java CryptEngineTest): the expected bytes were produced by the byte array implementation the Java login server used
// before it moved to Netty, so they pin down that the packets on the wire did not change; here at several offsets of a buffer holding garbage
namespace {

constexpr int32_t UPSTREAM_XOR_KEY = 0x1234ABCD;
constexpr std::string_view UPSTREAM_FIRST_PACKET =
	"80a5da452f7de4fc2fe5a9c81a35a221d40a2348a821e922d8ddc194ae825caf0f63bf2d7f48f750676cbec071018c120ab1a7b8962a510f";
constexpr std::string_view UPSTREAM_SECOND_PACKET = "84a289050547e7dca5e2c9e315c6ff2499c571619dfdca10";
constexpr std::string_view UPSTREAM_CLIENT_PLAIN = "0b000000112233441a2233441a223344";
constexpr std::string_view UPSTREAM_CLIENT_CIPHER = "f145b9b4084c3cfd7e5c1a0cb8cddfd7";

std::vector<uint8_t> parseHex(std::string_view hex) {
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i + 1 < hex.size(); i += 2)
		bytes.push_back(static_cast<uint8_t>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
	return bytes;
}

std::string formatHex(std::span<const uint8_t> bytes) {
	static constexpr char DIGITS[] = "0123456789abcdef";
	std::string hex;
	for (uint8_t b : bytes) {
		hex += DIGITS[b >> 4];
		hex += DIGITS[b & 0xF];
	}
	return hex;
}

void useUpstreamSessionKey(CryptEngine& engine) {
	std::vector<uint8_t> key(16);
	for (size_t i = 0; i < 16; i++)
		key[i] = static_cast<uint8_t>(0x31 + i * 5);
	engine.updateKey(key);
}

/** the bytes at the offset of a buffer filled with garbage, like a reused buffer */
std::vector<uint8_t> garbageBuffer(size_t offset, std::span<const uint8_t> bytes, size_t capacity) {
	std::vector<uint8_t> buf(capacity, 0x5A);
	std::ranges::copy(bytes, buf.begin() + static_cast<ptrdiff_t>(offset));
	return buf;
}

} // namespace

TEST(CryptEngineTest, EncryptsLikeTheFormerJavaImplementation) {
	std::vector<uint8_t> firstPlain(40);
	for (size_t i = 0; i < firstPlain.size(); i++)
		firstPlain[i] = static_cast<uint8_t>(i * 7 + 3);
	std::vector<uint8_t> secondPlain(13);
	for (size_t i = 0; i < secondPlain.size(); i++)
		secondPlain[i] = static_cast<uint8_t>(0xF0 - i * 11);
	for (size_t offset : {0, 2, 7}) {
		CryptEngine engine;
		useUpstreamSessionKey(engine);
		std::vector<uint8_t> first = garbageBuffer(offset, firstPlain, offset + 64);
		int32_t firstLength = engine.encrypt(std::span(first).subspan(offset), 40, UPSTREAM_XOR_KEY);
		ASSERT_EQ(firstLength, 56);
		EXPECT_EQ(formatHex(std::span(first).subspan(offset, 56)), UPSTREAM_FIRST_PACKET) << offset;

		std::vector<uint8_t> second = garbageBuffer(offset, secondPlain, offset + 32);
		int32_t secondLength = engine.encrypt(std::span(second).subspan(offset), 13, UPSTREAM_XOR_KEY);
		ASSERT_EQ(secondLength, 24);
		EXPECT_EQ(formatHex(std::span(second).subspan(offset, 24)), UPSTREAM_SECOND_PACKET) << offset;
	}
}

TEST(CryptEngineTest, DecryptsAndAcceptsAValidClientPacketOfTheFormerJavaImplementation) {
	for (size_t offset : {0, 2, 7}) {
		CryptEngine engine;
		useUpstreamSessionKey(engine);
		std::vector<uint8_t> init(64);
		engine.encrypt(init, 40, UPSTREAM_XOR_KEY); // SM_INIT switches the engine to the session key
		std::vector<uint8_t> packet = garbageBuffer(offset, parseHex(UPSTREAM_CLIENT_CIPHER), offset + 16);
		EXPECT_TRUE(engine.decrypt(std::span(packet).subspan(offset, 16)));
		EXPECT_EQ(formatHex(std::span(packet).subspan(offset, 16)), UPSTREAM_CLIENT_PLAIN);
	}
}

TEST(CryptEngineTest, RejectsACorruptedClientPacketOfTheFormerJavaImplementation) {
	CryptEngine engine;
		useUpstreamSessionKey(engine);
	std::vector<uint8_t> init(64);
	engine.encrypt(init, 40, UPSTREAM_XOR_KEY);
	std::vector<uint8_t> corrupted = parseHex(UPSTREAM_CLIENT_CIPHER);
	corrupted[3] ^= 0x40;
	EXPECT_FALSE(engine.decrypt(corrupted));
}
