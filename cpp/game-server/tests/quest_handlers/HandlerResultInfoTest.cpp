// P5-06b, M5d H-06 (m5d-plan.md §7, D17(a)): the HandlerResult companion - Java's static HandlerResult.fromBoolean(Boolean) as the free function
// ::aion::gameserver::questEngine::handlers::fromBoolean(std::optional<bool>) of HandlerResultInfo.h, the spelling the phase-6 transliterator
// emits.

#include <optional>

#include <gtest/gtest.h>

#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"

namespace aion::gameserver::questEngine::handlers::test {
namespace {

// HandlerResult.java:11-17: null is UNKNOWN (let the other handlers decide), true SUCCESS, false FAILED
TEST(HandlerResultInfoTest, FromBooleanMapsNullTrueAndFalse) {
	EXPECT_EQ(fromBoolean(std::nullopt), HandlerResult::UNKNOWN);
	EXPECT_EQ(fromBoolean(true), HandlerResult::SUCCESS);
	EXPECT_EQ(fromBoolean(false), HandlerResult::FAILED);
	EXPECT_EQ(::aion::gameserver::questEngine::handlers::fromBoolean(std::optional<bool>(false)), HandlerResult::FAILED)
		<< "the qualified spelling of the generated handlers (phase6-questgen-prototype.md §5.2 item 1)";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
