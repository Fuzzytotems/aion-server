#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>

#include "aion/loginserver/network/ncrypt/KeyGen.h"

using namespace aion::loginserver::network::ncrypt;

namespace {

/** key generation is slow in debug builds: most tests share one initialization */
void initOnce() {
	static std::once_flag once;
	std::call_once(once, KeyGen::init);
}

/** @return all distinct cached key pairs, found by sampling (the cache itself is not exposed, like in Java) */
std::set<std::shared_ptr<const EncryptedRSAKeyPair>> sampleKeyPairs() {
	std::set<std::shared_ptr<const EncryptedRSAKeyPair>> pairs;
	for (int i = 0; i < 2000 && pairs.size() < KeyGen::RSA_KEY_PAIR_COUNT; i++)
		pairs.insert(KeyGen::getEncryptedRSAKeyPair());
	// a few more samples must not find an 11th pair
	for (int i = 0; i < 200; i++)
		pairs.insert(KeyGen::getEncryptedRSAKeyPair());
	return pairs;
}

} // namespace

TEST(KeyGenTest, RsaKeyPairs) {
	initOnce();
	auto pairs = sampleKeyPairs();
	ASSERT_EQ(pairs.size(), KeyGen::RSA_KEY_PAIR_COUNT);

	std::set<std::vector<uint8_t>> moduli;
	for (const auto& pair : pairs) {
		ASSERT_NE(pair, nullptr);
		EVP_PKEY* key = pair->getRSAKeyPair();
		EXPECT_TRUE(EVP_PKEY_is_a(key, "RSA"));
		EXPECT_EQ(EVP_PKEY_get_bits(key), 1024);
		EXPECT_EQ(EVP_PKEY_get_size(key), 128);
		BIGNUM* e = nullptr;
		ASSERT_TRUE(EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_E, &e));
		BignumPtr exponent(e);
		EXPECT_TRUE(BN_is_word(exponent.get(), 65537));
		EXPECT_EQ(pair->getEncryptedModulus().size(), 128u);
		moduli.emplace(pair->getEncryptedModulus().begin(), pair->getEncryptedModulus().end());
	}
	EXPECT_EQ(moduli.size(), KeyGen::RSA_KEY_PAIR_COUNT);
}

TEST(KeyGenTest, ReinitReplacesPairsAndKeepsHandedOutOnesValid) {
	initOnce();
	auto old = sampleKeyPairs();
	KeyGen::init();
	auto current = sampleKeyPairs();
	ASSERT_EQ(current.size(), KeyGen::RSA_KEY_PAIR_COUNT);
	for (const auto& pair : old) {
		EXPECT_FALSE(current.contains(pair));
		EXPECT_EQ(pair->getEncryptedModulus().size(), 128u); // still alive
	}
}

TEST(KeyGenTest, BlowfishKeys) {
	initOnce();
	std::set<KeyGen::BlowfishKey> keys;
	for (int i = 0; i < 1000; i++)
		keys.insert(KeyGen::generateBlowfishKey());
	EXPECT_EQ(keys.size(), 1000u);
	EXPECT_EQ(sizeof(KeyGen::BlowfishKey), 16u);

	// every byte value position gets used (a rough check that all 16 bytes are random)
	std::array<std::set<uint8_t>, KeyGen::BLOWFISH_KEY_SIZE> values;
	for (const auto& key : keys) {
		for (size_t i = 0; i < key.size(); i++)
			values[i].insert(key[i]);
	}
	for (const auto& set : values)
		EXPECT_GT(set.size(), 200u);
}

TEST(KeyGenTest, ConcurrentAccess) {
	initOnce();
	std::vector<std::thread> threads;
	std::atomic<int> failures = 0;
	for (int t = 0; t < 4; t++) {
		threads.emplace_back([&] {
			for (int i = 0; i < 500; i++) {
				if (KeyGen::getEncryptedRSAKeyPair()->getEncryptedModulus().size() != 128)
					failures++;
				KeyGen::generateBlowfishKey();
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	EXPECT_EQ(failures.load(), 0);
}
