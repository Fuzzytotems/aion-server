#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/network/ncrypt/EncryptedRSAKeyPair.h"
#include "support/AionLoginClientCrypto.h"

using namespace aion::commons::utils;
using namespace aion::loginserver::network::ncrypt;
using aion::loginserver::test::AionLoginClientCrypto;

namespace {

EvpPkeyPtr generateKey(unsigned int bits) {
	EvpPkeyPtr key(EVP_RSA_gen(bits));
	if (!key)
		throwOpenSslException("EVP_RSA_gen failed");
	return key;
}

/** a 1024-bit key shared by the tests of this file (generating keys is slow in debug builds) */
const EncryptedRSAKeyPair& sharedPair() {
	static const EncryptedRSAKeyPair pair(generateKey(1024));
	return pair;
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

std::vector<uint8_t> randomBytes(size_t size) {
	std::vector<uint8_t> bytes(size);
	Rnd::nextBytes(bytes);
	return bytes;
}

/** raw RSA public key encryption with OpenSSL's EVP API (independent of the test helper's BIGNUM implementation) */
std::vector<uint8_t> openSslEncrypt(EVP_PKEY* key, std::span<const uint8_t> block) {
	EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr));
	if (!ctx || EVP_PKEY_encrypt_init(ctx.get()) <= 0 || EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_NO_PADDING) <= 0)
		throwOpenSslException("encrypt init failed");
	std::vector<uint8_t> out(128);
	size_t outLength = out.size();
	if (EVP_PKEY_encrypt(ctx.get(), out.data(), &outLength, block.data(), block.size()) <= 0)
		throwOpenSslException("encrypt failed");
	out.resize(outLength);
	return out;
}

} // namespace

TEST(EncryptedRSAKeyPairTest, EncryptModulusHandDerived) {
	// modulus bytes: C0 01 02 03 ... 7F (top bit set, so BigInteger.toByteArray() has 129 bytes and the leading 0x00 is removed)
	std::vector<uint8_t> modulus(128);
	for (size_t i = 0; i < modulus.size(); i++)
		modulus[i] = static_cast<uint8_t>(i);
	modulus[0] = 0xC0;

	std::vector<uint8_t> expected(128);
	for (size_t i = 0; i < 0x40; i++) {
		expected[i] = 0x40;                              // i ^ (0x40 + i)
		expected[0x40 + i] = static_cast<uint8_t>(i);    // (0x40 + i) ^ 0x40
	}
	// bytes 0-3 were swapped with 4D-50 before XORing
	expected[0x00] = 0x0D; // 4D ^ 40
	expected[0x01] = 0x0F; // 4E ^ 41
	expected[0x02] = 0x0D; // 4F ^ 42
	expected[0x03] = 0x13; // 50 ^ 43
	expected[0x0D] = 0x8D; // (0D ^ C0) ^ (34 ^ 74)
	expected[0x0E] = 0x4F; // (0E ^ 01) ^ 40
	expected[0x0F] = 0x4D; // (0F ^ 02) ^ 40
	expected[0x10] = 0x53; // (10 ^ 03) ^ 40
	expected[0x40] = 0x4D; // 40 ^ 0D
	expected[0x41] = 0x4E; // 41 ^ 0F
	expected[0x42] = 0x4F; // 42 ^ 0D
	expected[0x43] = 0x50; // 43 ^ 13
	expected[0x4D] = 0x4D; // C0 ^ 8D
	expected[0x4E] = 0x4E; // 01 ^ 4F
	expected[0x4F] = 0x4F; // 02 ^ 4D
	expected[0x50] = 0x50; // 03 ^ 53

	EXPECT_EQ(EncryptedRSAKeyPair::encryptModulus(modulus), expected);

	// leading zero bytes of the magnitude are irrelevant
	std::vector<uint8_t> padded(3, 0);
	padded.insert(padded.end(), modulus.begin(), modulus.end());
	EXPECT_EQ(EncryptedRSAKeyPair::encryptModulus(padded), expected);

	// the client recovers the modulus
	auto recovered = AionLoginClientCrypto::decryptModulus(expected);
	EXPECT_TRUE(std::ranges::equal(recovered, modulus));
}

TEST(EncryptedRSAKeyPairTest, EncryptModulusByteArrayLengths) {
	// top bit clear: toByteArray() has 128 bytes already
	std::vector<uint8_t> modulus = randomBytes(128);
	modulus[0] = 0x45;
	std::vector<uint8_t> scrambled = EncryptedRSAKeyPair::encryptModulus(modulus);
	ASSERT_EQ(scrambled.size(), 128u);
	EXPECT_TRUE(std::ranges::equal(AionLoginClientCrypto::decryptModulus(scrambled), modulus));

	// shorter than 128 bytes (Java: ArrayIndexOutOfBoundsException)
	EXPECT_THROW(EncryptedRSAKeyPair::encryptModulus(std::vector<uint8_t>(127, 0x7F)), IllegalArgumentException);
	// 127-byte magnitude with the top bit set: 128 bytes including the sign byte, which is kept
	std::vector<uint8_t> signByte(127, 0xFF);
	std::vector<uint8_t> scrambledSignByte = EncryptedRSAKeyPair::encryptModulus(signByte);
	ASSERT_EQ(scrambledSignByte.size(), 128u);
	std::vector<uint8_t> withSignByte{0};
	withSignByte.insert(withSignByte.end(), signByte.begin(), signByte.end());
	EXPECT_TRUE(std::ranges::equal(AionLoginClientCrypto::decryptModulus(scrambledSignByte), withSignByte));
}

TEST(EncryptedRSAKeyPairTest, EncryptModulusLongerThan128Bytes) {
	// 129-byte magnitude: toByteArray() has 129 bytes without leading 0x00 (or 130 with it), so nothing is removed and trailing bytes are kept
	std::vector<uint8_t> modulus = randomBytes(129);
	modulus[0] = 0x12;
	std::vector<uint8_t> scrambled = EncryptedRSAKeyPair::encryptModulus(modulus);
	ASSERT_EQ(scrambled.size(), 129u);
	EXPECT_EQ(scrambled[128], modulus[128]);
	EXPECT_TRUE(std::ranges::equal(AionLoginClientCrypto::decryptModulus(std::span(scrambled).first(128)), std::span(modulus).first(128)));

	modulus[0] = 0x92;
	scrambled = EncryptedRSAKeyPair::encryptModulus(modulus);
	ASSERT_EQ(scrambled.size(), 130u);
	EXPECT_EQ(scrambled[129], modulus[128]);
}

TEST(EncryptedRSAKeyPairTest, GeneratedKeyPair) {
	const EncryptedRSAKeyPair& pair = sharedPair();
	ASSERT_NE(pair.getRSAKeyPair(), nullptr);
	std::vector<uint8_t> modulus = modulusOf(pair.getRSAKeyPair());
	ASSERT_EQ(modulus.size(), 128u);
	ASSERT_EQ(pair.getEncryptedModulus().size(), 128u);
	EXPECT_TRUE(std::ranges::equal(pair.getEncryptedModulus(), EncryptedRSAKeyPair::encryptModulus(modulus)));
	EXPECT_FALSE(std::ranges::equal(pair.getEncryptedModulus(), modulus));
	EXPECT_TRUE(std::ranges::equal(AionLoginClientCrypto::decryptModulus(pair.getEncryptedModulus()), modulus));
}

TEST(EncryptedRSAKeyPairTest, DecryptRoundTrip) {
	const EncryptedRSAKeyPair& pair = sharedPair();
	std::vector<uint8_t> modulus = modulusOf(pair.getRSAKeyPair());
	for (int iteration = 0; iteration < 20; iteration++) {
		size_t blocks = static_cast<size_t>(Rnd::get(1, 3));
		std::vector<uint8_t> plain = randomBytes(blocks * 128);
		std::vector<uint8_t> encrypted;
		for (size_t block = 0; block < blocks; block++) {
			auto blockPlain = std::span(plain).subspan(block * 128, 128);
			blockPlain[0] = 0; // smaller than the modulus
			if (iteration % 2 == 0)
				std::fill_n(blockPlain.begin(), Rnd::get(0, 127), uint8_t{0}); // leading zeros must be kept in the output
			std::vector<uint8_t> c = openSslEncrypt(pair.getRSAKeyPair(), blockPlain);
			ASSERT_EQ(c.size(), 128u);
			encrypted.insert(encrypted.end(), c.begin(), c.end());
		}
		// the test helper's raw RSA gives the same ciphertext
		EXPECT_EQ(AionLoginClientCrypto::rsaEncrypt(modulus, plain), encrypted);

		std::optional<std::vector<uint8_t>> decrypted = pair.decrypt(encrypted);
		ASSERT_TRUE(decrypted.has_value());
		EXPECT_EQ(*decrypted, plain);
	}
}

TEST(EncryptedRSAKeyPairTest, DecryptFailures) {
	const EncryptedRSAKeyPair& pair = sharedPair();
	std::vector<uint8_t> modulus = modulusOf(pair.getRSAKeyPair());

	// Java: IllegalArgumentException("Bad arguments") for an incomplete block
	EXPECT_THROW(pair.decrypt(std::vector<uint8_t>(127)), IllegalArgumentException);
	EXPECT_THROW(pair.decrypt(std::vector<uint8_t>(129)), IllegalArgumentException);
	EXPECT_EQ(pair.decrypt({}), std::optional<std::vector<uint8_t>>(std::vector<uint8_t>()));

	// Java: BadPaddingException "Message is larger than modulus" -> CM_LOGIN returns null
	EXPECT_EQ(pair.decrypt(modulus), std::nullopt);
	EXPECT_EQ(pair.decrypt(std::vector<uint8_t>(128, 0xFF)), std::nullopt);
	std::vector<uint8_t> secondBlockInvalid(128, 0);
	secondBlockInvalid[127] = 5;
	secondBlockInvalid.insert(secondBlockInvalid.end(), modulus.begin(), modulus.end());
	EXPECT_EQ(pair.decrypt(secondBlockInvalid), std::nullopt);

	// the error queue is clear afterwards
	EXPECT_EQ(ERR_peek_error(), 0u);

	// modulus - 1 and 0 are valid
	std::vector<uint8_t> belowModulus = modulus;
	belowModulus[127]--; // an RSA modulus is odd, so no borrow
	EXPECT_TRUE(pair.decrypt(belowModulus).has_value());
	auto zero = pair.decrypt(std::vector<uint8_t>(128));
	ASSERT_TRUE(zero.has_value());
	EXPECT_EQ(*zero, std::vector<uint8_t>(128));
}

TEST(EncryptedRSAKeyPairTest, OtherKeys) {
	EXPECT_THROW(EncryptedRSAKeyPair{nullptr}, IllegalArgumentException);
	EXPECT_THROW(EncryptedRSAKeyPair{generateKey(512)}, IllegalArgumentException); // modulus too short to be scrambled

	EvpPkeyPtr ec(EVP_EC_gen("P-256"));
	ASSERT_TRUE(ec);
	EXPECT_THROW(EncryptedRSAKeyPair{std::move(ec)}, IllegalArgumentException);

	// a 2048-bit key can be scrambled (first 128 bytes), but not used with 128-byte blocks (Java: IllegalBlockSizeException)
	EncryptedRSAKeyPair large(generateKey(2048));
	EXPECT_EQ(large.getEncryptedModulus().size(), 257u);
	EXPECT_EQ(large.decrypt(std::vector<uint8_t>(128)), std::nullopt);
}

TEST(EncryptedRSAKeyPairTest, ConcurrentDecrypt) {
	const EncryptedRSAKeyPair& pair = sharedPair();
	std::vector<uint8_t> modulus = modulusOf(pair.getRSAKeyPair());
	std::vector<uint8_t> plain = randomBytes(256);
	plain[0] = 0;
	plain[128] = 0;
	std::vector<uint8_t> encrypted = AionLoginClientCrypto::rsaEncrypt(modulus, plain);

	std::atomic<int> failures = 0;
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; t++) {
		threads.emplace_back([&] {
			for (int i = 0; i < 25; i++) {
				auto decrypted = pair.decrypt(encrypted);
				if (!decrypted || *decrypted != plain)
					failures++;
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	EXPECT_EQ(failures.load(), 0);
}
