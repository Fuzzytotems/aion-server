#include "aion/commons/configuration/transformers/InetSocketAddressTransformer.h"

#include <gtest/gtest.h>

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

namespace {

utils::InetSocketAddress address(std::string host, uint16_t port) {
	return utils::InetSocketAddress{.host = std::move(host), .port = port};
}

std::string errorMessage(std::string_view value) {
	try {
		InetSocketAddressTransformer::parse(value);
	} catch (const utils::IllegalArgumentException& e) {
		return e.what();
	}
	return "<no exception>";
}

} // namespace

TEST(InetSocketAddressTransformerTest, HostnamesAndIPv4) {
	EXPECT_EQ(transform<utils::InetSocketAddress>("0.0.0.0:2106"), address("0.0.0.0", 2106));
	EXPECT_EQ(transform<utils::InetSocketAddress>("localhost:9014"), address("localhost", 9014));
	EXPECT_EQ(transform<utils::InetSocketAddress>("127.0.0.1:0"), address("127.0.0.1", 0));
	EXPECT_EQ(transform<utils::InetSocketAddress>("my-host.example.com:65535"), address("my-host.example.com", 65535));
	EXPECT_EQ(transform<utils::InetSocketAddress>("a.b.:1"), address("a.b.", 1));
	EXPECT_EQ(transform<utils::InetSocketAddress>("1abc:80"), address("1abc", 80)); // single label may start with a digit
	EXPECT_EQ(transform<utils::InetSocketAddress>("123:80"), address("123", 80));   // single numeric label
	EXPECT_EQ(transform<utils::InetSocketAddress>("01.02.03.04:80"), address("01.02.03.04", 80));
	EXPECT_EQ(transform<utils::InetSocketAddress>("localhost:0080"), address("localhost", 80));
	EXPECT_EQ(transform<utils::InetSocketAddress>("user@host:80"), address("host", 80)); // userinfo is ignored, like in Java
	EXPECT_EQ(typeName<utils::InetSocketAddress>(), "InetSocketAddress");
}

TEST(InetSocketAddressTransformerTest, IPv6) {
	EXPECT_EQ(InetSocketAddressTransformer::parse("[::1]:80"), address("::1", 80));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[::]:0"), address("::", 0));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[1:2:3:4:5:6:7:8]:1"), address("1:2:3:4:5:6:7:8", 1));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[2001:db8::ff00:42:8329]:7777"), address("2001:db8::ff00:42:8329", 7777));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[::ffff:192.168.1.1]:80"), address("::ffff:192.168.1.1", 80));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[1:2:3:4:5:6:1.2.3.4]:80"), address("1:2:3:4:5:6:1.2.3.4", 80));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[fe80::1%eth0]:80"), address("fe80::1%eth0", 80));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[1::]:80"), address("1::", 80));

	for (std::string_view invalid : {"[1:2:3:4:5:6:7]:1", "[1:2:3:4:5:6:7:8:9]:1", "[1::2:3:4:5:6:7:8]:1", "[::g]:80", "[12345::]:80", "[::1", "[]:80",
	                                 "[1.2.3.4]:80", "[::1%]:80", "[:::]:80", "[::1]x:80", "[::1]:8x"})
		EXPECT_EQ(errorMessage(invalid), "Malformed IPv6 address or port in authority: //" + std::string(invalid)) << invalid;
	EXPECT_EQ(errorMessage("[::1]"), "port out of range:-1");
}

TEST(InetSocketAddressTransformerTest, AnyLocalAddressSpellings) {
	// Java resolves the host, so every spelling of the wildcard address is InetAddress.isAnyLocalAddress() (game/chat server Config.load use it to
	// decide whether to discover the local IP for client advertisement)
	for (std::string_view value : {"[0:0:0:0:0:0:0:0]:7777", "[::0]:7777", "[0::]:7777", "[0000::0000]:7777", "[::0.0.0.0]:7777"}) {
		utils::InetSocketAddress address = InetSocketAddressTransformer::parse(value);
		EXPECT_EQ(address.host, "::") << value;
		EXPECT_TRUE(address.isAnyLocalAddress()) << value;
	}
	// IPv4-mapped addresses are Inet4Address in Java
	EXPECT_EQ(InetSocketAddressTransformer::parse("[::ffff:0.0.0.0]:7777"), address("0.0.0.0", 7777));
	EXPECT_TRUE(InetSocketAddressTransformer::parse("[::FFFF:0:0]:7777").isAnyLocalAddress());
	// other addresses keep their spelling
	EXPECT_EQ(InetSocketAddressTransformer::parse("[::ffff:1.2.3.4]:1"), address("::ffff:1.2.3.4", 1));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[0::1]:1"), address("0::1", 1));
	EXPECT_EQ(InetSocketAddressTransformer::parse("[::%1]:1"), address("::%1", 1));
	EXPECT_FALSE(InetSocketAddressTransformer::parse("[0::1]:1").isAnyLocalAddress());
}

TEST(InetSocketAddressTransformerTest, PortErrors) {
	EXPECT_EQ(errorMessage("localhost"), "port out of range:-1");
	EXPECT_EQ(errorMessage("localhost:"), "port out of range:-1");
	EXPECT_EQ(errorMessage("127.0.0.1"), "port out of range:-1");
	EXPECT_EQ(errorMessage("localhost:65536"), "port out of range:65536");
	EXPECT_EQ(errorMessage("localhost:2147483647"), "port out of range:2147483647");
	// Java's URI parser fails on the port and falls back to a registry-based authority without host
	EXPECT_EQ(errorMessage("localhost:2147483648"), "hostname can't be null");
	EXPECT_EQ(errorMessage("localhost:+80"), "hostname can't be null");
	EXPECT_EQ(errorMessage("localhost:80:90"), "hostname can't be null");
}

TEST(InetSocketAddressTransformerTest, HostErrors) {
	EXPECT_EQ(errorMessage(""), "Expected authority at index 2: //");
	for (std::string_view invalid : {"local host:80", "host_name:80", "1.2.3.4.5:80", "256.1.1.1:80", "abc.1:80", "-abc:80", "abc-:80", ":80",
	                                 "a..b:80", "host/x:80", "h\xC3\xA4st:80", " localhost:80", "localhost :80", "1.2.3.4x:80"})
		EXPECT_EQ(errorMessage(invalid), "hostname can't be null") << invalid;
}

TEST(InetSocketAddressTransformerTest, TransformWrapsErrors) {
	try {
		transform<utils::InetSocketAddress>("localhost");
		FAIL();
	} catch (const configuration::TransformationException& e) {
		EXPECT_STREQ(e.what(), "Error parsing \"localhost\" as InetSocketAddress");
	}
}
