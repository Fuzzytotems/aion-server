#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/NetworkUtils.h"

using namespace aion::commons::utils;

TEST(NetworkUtilsTest, CheckIPMatching) {
	EXPECT_TRUE(NetworkUtils::checkIPMatching("*", "10.2.88.12"));
	EXPECT_TRUE(NetworkUtils::checkIPMatching("*.*.*.*", "10.2.88.12"));
	EXPECT_TRUE(NetworkUtils::checkIPMatching("10.2.88.12-13", "10.2.88.12"));
	EXPECT_FALSE(NetworkUtils::checkIPMatching("10.2.88.13-125", "10.2.88.12"));
	EXPECT_TRUE(NetworkUtils::checkIPMatching("192.168.1.0-255", "192.168.1.200"));
	EXPECT_TRUE(NetworkUtils::checkIPMatching("10.*.88.12", "10.2.88.12"));
	EXPECT_FALSE(NetworkUtils::checkIPMatching("10.3.88.12", "10.2.88.12"));
}

TEST(NetworkUtilsTest, IntToIpString) {
	EXPECT_EQ(NetworkUtils::intToIpString(0x0100007F), "127.0.0.1");
	EXPECT_EQ(NetworkUtils::intToIpString(static_cast<int32_t>(0xFFFFFFFF)), "255.255.255.255");
}

TEST(NetworkUtilsTest, ToHex) {
	ByteBuffer buf = ByteBuffer::allocate(18);
	for (int i = 0; i < 18; i++)
		buf.put(static_cast<int8_t>('A' + i));
	std::string hex = NetworkUtils::toHex(buf);
	EXPECT_EQ(hex, "0000: 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F 50    ABCDEFGHIJKLMNOP\n"
								 "0010: 51 52                                              QR");
}

TEST(NetworkUtilsTest, ToHexRangeIsChecked) {
	ByteBuffer buf = ByteBuffer::allocate(32);
	buf.put(static_cast<int8_t>('A')).put(static_cast<int8_t>('B')).put(static_cast<int8_t>('C'));
	buf.flip(); // limit 3, capacity 32
	EXPECT_EQ(NetworkUtils::toHex(buf, 1, 3), "0000: 42 43                                              BC");
	EXPECT_EQ(NetworkUtils::toHex(buf, 2, 2), "");
	EXPECT_EQ(NetworkUtils::toHex(buf, -5, -5), "");
	EXPECT_THROW(NetworkUtils::toHex(buf, -1, 3), IndexOutOfBoundsException); // would read before the buffer
	EXPECT_THROW(NetworkUtils::toHex(buf, 0, 4), IndexOutOfBoundsException);  // beyond the limit (stale bytes)
}
