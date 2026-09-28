#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/InetSocketAddress.h"
#include "aion/commons/utils/NetworkUtils.h"

using namespace aion::commons::utils;

namespace {

InetSocketAddress address(std::string host) {
	return InetSocketAddress{.host = std::move(host), .port = 2106};
}

} // namespace

TEST(InetSocketAddressTest, IsAnyLocalAddress) {
	// Java: InetAddress.isAnyLocalAddress() of the resolved literal
	for (const char* host : {"", "0.0.0.0", "::", "0:0:0:0:0:0:0:0", "::0", "0000::0000", "::ffff:0.0.0.0", "0:0:0:0:0:ffff:0:0"})
		EXPECT_TRUE(address(host).isAnyLocalAddress()) << host;
	for (const char* host : {"127.0.0.1", "::1", "192.168.1.10", "::ffff:127.0.0.1", "localhost", "0.0.0.0.example.org", "0.0.0.1", "fe80::1%1"})
		EXPECT_FALSE(address(host).isAnyLocalAddress()) << host;
	EXPECT_EQ(address("::0").getAddressInfo(), "all addresses on port 2106");
	EXPECT_EQ(address("127.0.0.1").getAddressInfo(), "127.0.0.1:2106");
}

TEST(InetSocketAddressTest, ResolveAddressBytes) {
	EXPECT_EQ(address("127.0.0.1").resolveAddressBytes(), (std::vector<uint8_t>{127, 0, 0, 1}));
	EXPECT_EQ(address("192.168.1.200").resolveAddressBytes(), (std::vector<uint8_t>{192, 168, 1, 200}));
	EXPECT_EQ(address("::ffff:10.0.0.1").resolveAddressBytes(), (std::vector<uint8_t>{10, 0, 0, 1})); // Java: Inet4Address
	EXPECT_EQ(address("::1").resolveAddressBytes(), (std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
	EXPECT_EQ(address("").resolveAddressBytes(), (std::vector<uint8_t>{0, 0, 0, 0}));
	EXPECT_EQ(address("localhost").resolveAddressBytes(), (std::vector<uint8_t>{127, 0, 0, 1})); // IPv4 preferred over ::1
	EXPECT_THROW(address("no-such-host.invalid").resolveAddressBytes(), IOException);
}

TEST(InetSocketAddressTest, LocalIPv4CanBeAdvertised) {
	// game server Config: CLIENT_CONNECT_ADDRESS = new InetSocketAddress(NetworkUtils.findLocalIPv4(), port), then SM_GS_AUTH writes the bytes
	std::optional<std::string> localIPv4 = NetworkUtils::findLocalIPv4();
	if (!localIPv4)
		GTEST_SKIP() << "no network";
	InetSocketAddress advertised{.host = *localIPv4, .port = 7777};
	EXPECT_FALSE(advertised.isAnyLocalAddress());
	EXPECT_EQ(advertised.resolveAddressBytes().size(), 4u);
}
