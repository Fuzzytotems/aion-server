#include <gtest/gtest.h>

#include <magic_enum/magic_enum.hpp>

// what the network headers do: some Asio headers (and therefore <windows.h> on Windows) first, then the guard
#include <asio/ip/tcp.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

#include "aion/commons/utils/WindowsMacroGuard.h"

// Asio headers included for the first time after the guard still compile and the macros stay removed. The complete <asio.hpp> covers
// Asio's inline Windows implementation, which uses INFINITE, and <bcrypt.h> (asio/impl/connect_pipe.ipp), which defines IN/OUT/OPTIONAL
// again unless they are already defined.
#include <asio.hpp>
#include <cmath>

// a second inclusion (e.g. via several headers) is harmless
#include "aion/commons/utils/WindowsMacroGuard.h"

namespace {

/** Names of Java enum values and parameters (see WindowsMacroGuard.h) that collide with Windows macros. */
enum class CollidingNames {
	IN,
	OUT,
	OPTIONAL,
	ERROR,
	ABSOLUTE,
	RELATIVE,
	TRANSPARENT,
	OPAQUE,
	ALTERNATE,
	WINDING,
	DIFFERENCE,
	DELETE,
	IGNORE,
	NO_ERROR,
	KEY_EVENT,
	MOUSE_EVENT,
	FOCUS_EVENT,
	MENU_EVENT,
	MOUSE_MOVED,
	DOUBLE_CLICK,
	DOMAIN,
	SING,
	OVERFLOW,
	UNDERFLOW,
	TLOSS,
	PLOSS,
};

float frustumDepth(float near, float far) {
	return far - near;
}

int max(int a, int b) {
	return a > b ? a : b;
}

} // namespace

TEST(WindowsMacroGuardTest, CollidingNamesCanBeUsed) {
	EXPECT_EQ(magic_enum::enum_name(CollidingNames::ERROR), "ERROR");
	EXPECT_EQ(magic_enum::enum_name(CollidingNames::DELETE), "DELETE");
	EXPECT_EQ(magic_enum::enum_name(CollidingNames::IGNORE), "IGNORE");
	EXPECT_EQ(magic_enum::enum_name(CollidingNames::IN), "IN");
	EXPECT_EQ(magic_enum::enum_cast<CollidingNames>("OUT"), CollidingNames::OUT);
	EXPECT_EQ(magic_enum::enum_count<CollidingNames>(), 26u);
	EXPECT_EQ(frustumDepth(1.0f, 11.0f), 10.0f);
	EXPECT_EQ(max(1, 2), 2);
}

TEST(WindowsMacroGuardTest, WindowsAndAsioStillWork) {
	asio::io_context context;
	asio::steady_timer timer(context, std::chrono::milliseconds(1));
	bool fired = false;
	timer.async_wait([&](const asio::error_code& error) { fired = !error; });
	context.run();
	EXPECT_TRUE(fired);
	asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 7777);
	EXPECT_EQ(endpoint.port(), 7777);
#ifdef _WIN32
	EXPECT_NE(GetCurrentProcessId(), 0u);
	EXPECT_EQ(INFINITE, 0xFFFFFFFFul); // kept for Asio
	EXPECT_EQ(std::sqrt(4.0), 2.0);
#endif
}
