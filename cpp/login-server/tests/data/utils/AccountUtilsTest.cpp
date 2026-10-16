#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "aion/loginserver/utils/AccountUtils.h"

using namespace aion::loginserver::utils;

TEST(AccountUtilsTest, EncodePasswordKnownVectors) {
	// Base64(SHA-1(UTF-8 bytes)), like Java's AccountUtils.encodePassword
	EXPECT_EQ(AccountUtils::encodePassword("password"), "W6ph5Mm5Pz8GgiULbPgzG37mj9g=");
	EXPECT_EQ(AccountUtils::encodePassword(""), "2jmj7l5rSw0yVb/vlWAYkK/YBwk=");
	EXPECT_EQ(AccountUtils::encodePassword("abc"), "qZk+NkcGgWq6PiVxeFDCbJzQ2J0=");
	EXPECT_EQ(AccountUtils::encodePassword("admin"), "0DPiKuNIrrVmD8IUCuw1hQxNqZc=");
	EXPECT_EQ(AccountUtils::encodePassword("Gr\xC3\xBC\xC3\x9F" "e \xF0\x9F\x98\x80"), "SNFm0PdbvNSDU1M3X/yhoP72v/Q="); // "Grüße 😀"
}

TEST(AccountUtilsTest, EncodePasswordHandlesEmbeddedZeroAndLongInput) {
	std::string withZero("a\0b", 3);
	EXPECT_NE(AccountUtils::encodePassword(withZero), AccountUtils::encodePassword("a"));
	std::string encoded = AccountUtils::encodePassword(std::string(100000, 'x'));
	EXPECT_EQ(encoded.size(), 28u);
}

TEST(AccountUtilsTest, EncodePasswordIsThreadSafe) {
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; t++) {
		threads.emplace_back([] {
			for (int i = 0; i < 1000; i++)
				EXPECT_EQ(AccountUtils::encodePassword("password"), "W6ph5Mm5Pz8GgiULbPgzG37mj9g=");
		});
	}
	for (auto& thread : threads)
		thread.join();
}
