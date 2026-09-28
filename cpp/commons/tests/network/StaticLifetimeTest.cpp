#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>

#include "NetworkTestUtils.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/PacketProcessor.h"

// Java servers hold their PacketProcessor (and NioServer) in static fields, e.g. LoginConnection:
// "private final static PacketProcessor<LoginConnection> processor = new PacketProcessor<>(1, 8, 50, 3)". A mechanical port creates them during
// static initialization, possibly before the namespace scope statics of the network library are initialized, and destroys them after those were
// destroyed.

#ifdef _MSC_VER
// Initialize the statics of this file before all ordinary namespace scope statics (e.g. those of the network library), which makes the
// problematic order deterministic instead of depending on the link order.
#pragma warning(disable : 4073) // initializers put in library initialization area
#pragma init_seg(lib)
#endif

using namespace nettest;
using namespace aion::commons;

namespace {

struct StaticConnection {
	std::string toString() const { return "StaticConnection"; }
};

std::atomic<bool> processorConstructed = false;

// constructed during static initialization of this test executable, before main
network::PacketProcessor<StaticConnection> staticProcessor = [] {
	processorConstructed = true;
	return network::PacketProcessor<StaticConnection>(1, 2, 50, 3);
}();

// started in a test, shut down by its destructor after main returned (logs "Closing ServerChannels...")
network::NioServer staticServer(1, {network::ServerCfg{{"127.0.0.1", 0}, "static clients", {}}});

} // namespace

TEST(StaticLifetimeTest, PacketProcessorCanBeConstructedDuringStaticInitialization) {
	EXPECT_TRUE(processorConstructed);
	EXPECT_EQ(staticProcessor.getThreadCount(), 1);
}

TEST(StaticLifetimeTest, NioServerCanBeShutDownDuringStaticDestruction) {
	static std::once_flag started; // --gtest_repeat runs this test several times
	std::call_once(started, [] { staticServer.connect(); });
	EXPECT_EQ(staticServer.getBoundAddresses().size(), 1u);
}
