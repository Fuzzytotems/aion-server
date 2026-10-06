// P5-09a DropDistributionService (m5g-plan.md L-01, L-04) on the party fixture of tests/team/P5-10b (by relative path): a team corpse's roll and
// bid prompts answered by every in-range member with a pass - the per-answer messages, SM_GROUP_LOOT per answer and the final one with
// player 1 and luck 0xFFFFFFFF, STR_MSG_PAY_ALL_GIVEUP, the item free for all and out of the distribution queue - and a bid above the
// bidder's kinah read as 0. No database: the corpse is registered in DropRegistrationService's maps by hand, as registerDrop leaves it.
//
// A winner's path (distributeLoot -> DropService.requestDropItem -> the item into the winner's inventory) is the gate's (GP13); the strict
// greater-than of a tie is a mutation the gate cannot kill reliably either (m5g-plan.md §10.4) and stays open here.
//
// Expectations are derived by hand from DropDistributionService.java:29-146.

#include "../../team/P5-10b/TeamTestSupport.h"

#include <cstdint>

#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_LOOT.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/services/drop/DropDistributionService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::drop::DropItem;
using model::gameobjects::DropNpc;
using serverpackets::SM_GROUP_LOOT;
using services::drop::DropDistributionService;
using services::drop::DropRegistrationService;

constexpr int32_t CORPSE = 739001;
constexpr int32_t POTION = 162000002;

class DropDistributionServiceTest : public TeamTest {
protected:
	void TearDown() override {
		DropRegistrationService::getInstance().getDropRegistrationMap().remove(CORPSE);
		DropRegistrationService::getInstance().getCurrentDropMap().remove(CORPSE);
		item = nullptr;
		dropNpc = nullptr;
		TeamTest::TearDown();
	}

	/** a corpse of the group's kill with one RARE potion at index 1 waiting for `distributionId` (2 roll, 3 bid) from A and B */
	void registerCorpse(PlayerGroup& group, Member& a, Member& b, int32_t distributionId) {
		dropNpc = DropNpc::create(CORPSE);
		dropNpc->setLootingTeam(group);
		dropNpc->setDistributionId(distributionId);
		dropNpc->setCurrentIndex(1);
		runtime::Ref<runtime::RcArrayList<runtime::Ref<Player>>> inRange =
			runtime::RcArrayList<runtime::Ref<Player>>::create(AION_LOCK_CLASS(DropNpc::inRangePlayers));
		inRange->add(runtime::Ref<Player>(a.player()));
		inRange->add(runtime::Ref<Player>(b.player()));
		dropNpc->setInRangePlayers(inRange);
		dropNpc->addPlayerStatus(a.player());
		dropNpc->addPlayerStatus(b.player());
		item = DropItem::create(model::drop::Drop(POTION, 1, 1, 100.0f));
		item->setIndex(1);
		item->setCount(1);
		group.getLootGroupRules()->addItemToBeDistributed(*item);
		runtime::Ref<runtime::RcHashSet<runtime::Ref<DropItem>>> items =
			runtime::RcHashSet<runtime::Ref<DropItem>>::create(AION_LOCK_CLASS(DropRegistrationService::currentDropMap#dropItems));
		items->add(item);
		DropRegistrationService::getInstance().getDropRegistrationMap().put(CORPSE, dropNpc);
		DropRegistrationService::getInstance().getCurrentDropMap().put(CORPSE, items);
	}

	runtime::Ref<DropNpc> dropNpc;
	runtime::Ref<DropItem> item;
};

/** handleRoll with roll 0 from both: STR_MSG_DICE_GIVEUP_ME / _OTHER, then everybody passed (DropDistributionService.java:57-75, 99-130) */
TEST_F(DropDistributionServiceTest, EveryonePassesTheRollAndTheItemTurnsFreeForAll) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerGroup& group = form({&a, &b});
	registerCorpse(group, a, b, 2);
	DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<Player>(a.player()), 2, 0, 0, POTION, CORPSE, 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_DICE_GIVEUP_ME()), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_MSG_DICE_GIVEUP_OTHER("Alpha")), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_PAY_ALL_GIVEUP()), 0) << "B has not answered yet";
	EXPECT_FALSE(item->isFreeForAll());
	DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<Player>(b.player()), 2, 0, 0, POTION, CORPSE, 1);
	for (Member* m : {&a, &b}) {
		EXPECT_EQ(m->count(SM_SYSTEM_MESSAGE::STR_MSG_PAY_ALL_GIVEUP()), 1) << m->player().getName();
		EXPECT_EQ(m->count(SM_GROUP_LOOT(group.getTeamId(), 1, POTION, 1, CORPSE, 2, static_cast<int32_t>(0xFFFFFFFF), 1)), 1)
			<< "the final SM_GROUP_LOOT: player 1 for nobody, luck 0xFFFFFFFF";
	}
	EXPECT_TRUE(item->isFreeForAll());
	EXPECT_FALSE(item->getWinningPlayer());
	EXPECT_FALSE(group.getLootGroupRules()->containDropItem(*item)) << "removeItemToBeDistributed";
}

/** handleBid (DropDistributionService.java:78-97): a bid above the bidder's kinah is read as 0, a pass */
TEST_F(DropDistributionServiceTest, ABidAboveTheKinahIsAPass) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerGroup& group = form({&a, &b});
	giveKinah(a, 739101, 10);
	giveKinah(b, 739102, 0);
	registerCorpse(group, a, b, 3);
	DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<Player>(a.player()), 3, 0, 50, POTION, CORPSE, 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_PAY_GIVEUP_ME()), 1) << "50 > 10 kinah: bid = 0";
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_MSG_PAY_GIVEUP_OTHER("Alpha")), 1);
	DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<Player>(b.player()), 3, 0, 0, POTION, CORPSE, 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_PAY_ALL_GIVEUP()), 1);
	EXPECT_TRUE(item->isFreeForAll());
}

/** handleRollOrBid ignores an unknown corpse and logs an unknown mode (DropDistributionService.java:29-54) */
TEST_F(DropDistributionServiceTest, AnUnknownCorpseOrModeDoesNothing) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerGroup& group = form({&a, &b});
	DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<Player>(a.player()), 2, 1, 0, POTION, CORPSE, 1);
	EXPECT_TRUE(a.sent().empty());
	registerCorpse(group, a, b, 2);
	DropDistributionService::getInstance().handleRollOrBid(runtime::Ptr<Player>(a.player()), 5, 1, 0, POTION, CORPSE, 1);
	EXPECT_TRUE(a.sent().empty());
	EXPECT_TRUE(dropNpc->containsPlayerStatus(a.player()));
	DropDistributionService::getInstance().handleRollOrBid(nullptr, 2, 1, 0, POTION, CORPSE, 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
