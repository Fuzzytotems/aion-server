#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

#include <openssl/bn.h>

#include "aion/clientprotocol/Rsa.h"

// The library's 1024-bit modular exponentiation against OpenSSL's BN_mod_exp (the code it replaces in the client side of CM_LOGIN).

namespace aion::clientprotocol {
namespace {

using BignumPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;
using BnCtxPtr = std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)>;

BignumPtr bignum(std::span<const uint8_t> bytes) {
	return BignumPtr(BN_bin2bn(bytes.data(), static_cast<int>(bytes.size()), nullptr), &BN_free);
}

std::array<uint8_t, rsa::BLOCK_SIZE> openSslModPow(std::span<const uint8_t> base, uint32_t exponent, std::span<const uint8_t> modulus) {
	BnCtxPtr ctx(BN_CTX_new(), &BN_CTX_free);
	BignumPtr b = bignum(base);
	BignumPtr n = bignum(modulus);
	BignumPtr e(BN_new(), &BN_free);
	BignumPtr r(BN_new(), &BN_free);
	if (!ctx || !b || !n || !e || !r || !BN_set_word(e.get(), exponent) || !BN_mod_exp(r.get(), b.get(), e.get(), n.get(), ctx.get()))
		throw std::runtime_error("OpenSSL BN_mod_exp failed");
	std::array<uint8_t, rsa::BLOCK_SIZE> result{};
	if (BN_bn2binpad(r.get(), result.data(), static_cast<int>(result.size())) != static_cast<int>(result.size()))
		throw std::runtime_error("OpenSSL BN_bn2binpad failed");
	return result;
}

std::vector<uint8_t> randomBytes(std::mt19937& random, size_t size) {
	std::vector<uint8_t> bytes(size);
	for (uint8_t& b : bytes)
		b = static_cast<uint8_t>(random());
	return bytes;
}

/** a random modulus of exactly `bits` bits (top bit set), odd or even */
std::vector<uint8_t> randomModulus(std::mt19937& random, size_t bits) {
	std::vector<uint8_t> modulus = randomBytes(random, (bits + 7) / 8);
	const size_t topBit = (bits - 1) % 8;
	modulus[0] &= static_cast<uint8_t>((1u << (topBit + 1)) - 1);
	modulus[0] |= static_cast<uint8_t>(1u << topBit);
	return modulus;
}

/** a random number below the modulus (same length, top byte below the modulus' top byte) */
std::vector<uint8_t> randomBelow(std::mt19937& random, const std::vector<uint8_t>& modulus) {
	std::vector<uint8_t> base = randomBytes(random, modulus.size());
	base[0] = modulus[0] == 0 ? 0 : static_cast<uint8_t>(random() % modulus[0]);
	return base;
}

TEST(RsaTest, PublicExponentMatchesOpenSsl) {
	std::mt19937 random(4808);
	for (int i = 0; i < 40; i++) {
		const std::vector<uint8_t> modulus = randomModulus(random, 1024);
		const std::vector<uint8_t> base = randomBelow(random, modulus);
		EXPECT_EQ(rsa::modPow(base, rsa::PUBLIC_EXPONENT, modulus), openSslModPow(base, rsa::PUBLIC_EXPONENT, modulus)) << "case " << i;
	}
}

TEST(RsaTest, OtherExponentsAndSizesMatchOpenSsl) {
	std::mt19937 random(207);
	const std::array<uint32_t, 7> exponents = {0, 1, 2, 3, 17, 0x80000001u, 0xFFFFFFFFu};
	const std::array<size_t, 6> sizes = {2, 31, 64, 513, 1000, 1024};
	for (size_t bits : sizes) {
		for (uint32_t exponent : exponents) {
			const std::vector<uint8_t> modulus = randomModulus(random, bits);
			const std::vector<uint8_t> base = randomBelow(random, modulus);
			EXPECT_EQ(rsa::modPow(base, exponent, modulus), openSslModPow(base, exponent, modulus)) << bits << " bits, exponent " << exponent;
		}
	}
}

TEST(RsaTest, EdgeValuesMatchOpenSsl) {
	std::vector<uint8_t> max(rsa::BLOCK_SIZE, 0xFF); // 2^1024 - 1
	std::vector<uint8_t> belowMax = max;
	belowMax.back() = 0xFE;
	const std::vector<uint8_t> zero(rsa::BLOCK_SIZE, 0);
	std::vector<uint8_t> one(rsa::BLOCK_SIZE, 0);
	one.back() = 1;
	for (const std::vector<uint8_t>& base : {zero, one, belowMax})
		EXPECT_EQ(rsa::modPow(base, rsa::PUBLIC_EXPONENT, max), openSslModPow(base, rsa::PUBLIC_EXPONENT, max));
	// a modulus of 1: everything is 0
	EXPECT_EQ(rsa::modPow(zero, 0, one), (std::array<uint8_t, rsa::BLOCK_SIZE>{}));
	// leading zero bytes and a short base are accepted
	std::vector<uint8_t> padded(200, 0);
	padded.insert(padded.end(), max.begin(), max.end());
	const std::vector<uint8_t> shortBase = {0x12, 0x34};
	EXPECT_EQ(rsa::modPow(shortBase, rsa::PUBLIC_EXPONENT, padded), openSslModPow(shortBase, rsa::PUBLIC_EXPONENT, max));
}

TEST(RsaTest, EncryptEncryptsEachBlock) {
	std::mt19937 random(1252);
	const std::vector<uint8_t> modulus = randomModulus(random, 1024);
	std::vector<uint8_t> plain = randomBelow(random, modulus);
	const std::vector<uint8_t> second = randomBelow(random, modulus);
	plain.insert(plain.end(), second.begin(), second.end());
	const std::vector<uint8_t> encrypted = rsa::encrypt(modulus, plain);
	ASSERT_EQ(encrypted.size(), 2 * rsa::BLOCK_SIZE);
	const auto first = openSslModPow(std::span(plain).first(rsa::BLOCK_SIZE), rsa::PUBLIC_EXPONENT, modulus);
	const auto last = openSslModPow(std::span(plain).last(rsa::BLOCK_SIZE), rsa::PUBLIC_EXPONENT, modulus);
	EXPECT_TRUE(std::equal(first.begin(), first.end(), encrypted.begin()));
	EXPECT_TRUE(std::equal(last.begin(), last.end(), encrypted.begin() + rsa::BLOCK_SIZE));
}

TEST(RsaTest, InvalidInput) {
	std::vector<uint8_t> modulus(rsa::BLOCK_SIZE, 0x80);
	EXPECT_THROW(rsa::encrypt(modulus, std::vector<uint8_t>(100)), std::invalid_argument);
	EXPECT_THROW(rsa::encrypt(modulus, modulus), std::invalid_argument); // not smaller than the modulus
	EXPECT_THROW(rsa::modPow(std::vector<uint8_t>{1}, 3, std::vector<uint8_t>(rsa::BLOCK_SIZE, 0)), std::invalid_argument); // modulus 0
	EXPECT_THROW(rsa::modPow(std::vector<uint8_t>{1}, 3, std::vector<uint8_t>{}), std::invalid_argument);
	EXPECT_THROW(rsa::modPow(std::vector<uint8_t>{1}, 3, std::vector<uint8_t>(rsa::BLOCK_SIZE + 1, 1)), std::invalid_argument); // 1032 bits
}

} // namespace
} // namespace aion::clientprotocol
