// P5-08 RecallService (m5g-plan.md W-06, D15: summon group member, skill 3777): requestSummon's question and its 30 s timeout on the
// deterministic clock, the decline, a second request for a player who already has one, validateCast's refusals and canBeSummoned's rules, on
// real Players of the party fixture (tests/team/P5-10b, by relative path).
//
// Expectations are derived by hand from RecallService.java:56-174.

#include "../team/P5-10b/TeamTestSupport.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/network/aion/serverpackets/SM_RECALLED_BY_OTHER.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using serverpackets::SM_RECALLED_BY_OTHER;
using services::RecallService;

class RecallServiceTest : public TeamTest {
protected:
	void TearDown() override {
		for (Member& m : members) {
			RecallService::getInstance().cancel(*m.f.player, RecallService::CancelReason::DECLINED);
			world::World::getInstance().removeObject(*m.f.player);
		}
		TeamTest::TearDown();
	}

	/** addMember plus World.storeObject: RecallService.cancel finds the caster through World.getPlayer(id) (RecallService.java:91) */
	Member& addStoredMember(std::string_view name, float x = 100.0f) {
		Member& m = addMember(name, x);
		world::World::getInstance().storeObject(*m.f.player);
		return m;
	}
};

/** RecallService.java:56-66: the summoned player is asked; 30 s later the request times out and the caster is told */
TEST_F(RecallServiceTest, ARequestAsksTheSummonedPlayerAndTimesOutAfterThirtySeconds) {
	Member& a = addStoredMember("Alpha");
	Member& c = addStoredMember("Charlie", 300.0f);
	RecallService::getInstance().requestSummon(a.player(), c.player(), 3777);
	EXPECT_TRUE(RecallService::getInstance().hasPendingRequest(c.player()));
	EXPECT_EQ(c.count(SM_RECALLED_BY_OTHER(std::optional<std::string_view>("Alpha"), 3777, 30)), 1);
	executor->advance(std::chrono::milliseconds(29'000));
	EXPECT_TRUE(RecallService::getInstance().hasPendingRequest(c.player()));
	executor->advance(std::chrono::milliseconds(1'500));
	EXPECT_FALSE(RecallService::getInstance().hasPendingRequest(c.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_Recall_DONOT_ACCEPT_EFFECT("Charlie")), 1);
	EXPECT_EQ(c.count(SM_RECALLED_BY_OTHER()), 1) << "the timeout closes the window";
}

/** A decline (CancelReason.DECLINED) tells both sides; the cancelled timeout never fires */
TEST_F(RecallServiceTest, ADeclineTellsBothSidesAndCancelsTheTimeout) {
	Member& a = addStoredMember("Alpha");
	Member& c = addStoredMember("Charlie", 300.0f);
	RecallService::getInstance().requestSummon(a.player(), c.player(), 3777);
	RecallService::getInstance().cancel(c.player(), RecallService::CancelReason::DECLINED);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_Recall_Rejected_EFFECT("Charlie")), 1);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_MSG_Recall_Reject_EFFECT("Alpha")), 1);
	clearAll();
	executor->advance(std::chrono::milliseconds(31'000));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_Recall_DONOT_ACCEPT_EFFECT("Charlie")), 0);
}

/** requests.putIfAbsent: a second request for the same player is dropped without a question (RecallService.java:59-60) */
TEST_F(RecallServiceTest, ASecondRequestForTheSamePlayerIsDropped) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	Member& c = addStoredMember("Charlie", 300.0f);
	RecallService::getInstance().requestSummon(a.player(), c.player(), 3777);
	clearAll();
	RecallService::getInstance().requestSummon(b.player(), c.player(), 3777);
	EXPECT_TRUE(c.sent().empty());
}

/**
 * canBeSummoned (RecallService.java:146-160): never the caster himself, only in the caster's map instance. validateCast's own order (flying,
 * canRecallAt, the target, a pending request, canBeSummoned) is the gate's (GP13b): canRecallAt asks the World's map, which this fixture leaves
 * out.
 */
TEST_F(RecallServiceTest, CanBeSummonedOnlyInTheCastersInstanceAndNeverHimself) {
	Member& a = addStoredMember("Alpha");
	Member& c = addStoredMember("Charlie", 300.0f);
	EXPECT_TRUE(RecallService::canBeSummoned(a.player(), c.player()));
	EXPECT_FALSE(RecallService::canBeSummoned(a.player(), a.player())) << "caster == summoned";
	c.player().setPosition(world::WorldPosition::create(210020000, 300.0f, 100.0f, 50.0f, int8_t{0}));
	EXPECT_FALSE(RecallService::canBeSummoned(a.player(), c.player())) << "another map";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
