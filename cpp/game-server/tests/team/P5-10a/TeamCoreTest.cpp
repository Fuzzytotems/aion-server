// P5-10a team core (m5g-plan.md C-09): GeneralTeam.onEvent's skip path and its reentrant lock under nested events, applyOnMembers' stop,
// TemporaryPlayerTeam.getOnlineMembers with an offline member (the source of the aura and heal-on-attacked team arms), the kinah split with its
// lost remainder, the TeamCommand and LootGroupRules companions and PlayerTeamCommandService's member lookup.
//
// Expectations are derived by hand from GeneralTeam.java:37-123, TemporaryPlayerTeam.java:55-75, TeamKinahDistributionEvent.java:24-50,
// TeamCommand.java:10-56, LootGroupRules.java:32-80 and PlayerTeamCommandService.java:22-90. The party fixture is tests/team/P5-10b's.

#include "../P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/TeamEvent.h"
#include "aion/gameserver/model/team/common/events/TeamCommand.h"
#include "aion/gameserver/model/team/common/events/TeamCommandInfo.h"
#include "aion/gameserver/model/team/common/events/TeamKinahDistributionEvent.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/common/service/PlayerTeamCommandService.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::team::TeamEvent;
using model::team::common::events::TeamCommand;
using model::team::common::events::TeamKinahDistributionEvent;
using model::team::common::legacy::LootGroupRules;
using model::team::common::legacy::LootRuleType;
using model::templates::item::ItemQuality;

class TeamCoreTest : public TeamTest {};

/** A TeamEvent that records whether it ran and whether the team lock was held */
class RecordingEvent final : public TeamEvent {
public:
	explicit RecordingEvent(bool condition) : condition(condition) {}
	bool checkCondition() override { return condition; }
	void handleEvent() override { handled = true; }
	bool condition;
	bool handled = false;
};

/** GeneralTeam.java:37-48: a false condition skips the event (and only logs) */
TEST_F(TeamCoreTest, OnEventSkipsAnEventWhoseConditionFails) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerGroup& group = form({&a, &b});
	RecordingEvent skipped(false);
	group.onEvent(skipped);
	EXPECT_FALSE(skipped.handled);
	RecordingEvent run(true);
	group.onEvent(run);
	EXPECT_TRUE(run.handled);
}

/** The team lock is reentrant: an event handled under onEvent runs another onEvent inside forEach (GroupDisbandEvent's shape) */
TEST_F(TeamCoreTest, NestedEventsRunUnderTheReentrantTeamLock) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	int32_t nested = 0;
	class Outer final : public TeamEvent {
	public:
		Outer(PlayerGroup& group, int32_t& nested) : group(group), nested(nested) {}
		bool checkCondition() override { return true; }
		void handleEvent() override {
			group.forEach([this](model::gameobjects::AionObject&) {
				RecordingEvent inner(true);
				group.onEvent(inner);
				nested += inner.handled ? 1 : 0;
			});
		}
		PlayerGroup& group;
		int32_t& nested;
	} outer(group, nested);
	group.onEvent(outer);
	EXPECT_EQ(nested, 3);
}

/** GeneralTeam.applyOnMembers stops at the first false (GeneralTeam.java:104-117) */
TEST_F(TeamCoreTest, ApplyOnMembersStopsAtTheFirstFalse) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	int32_t visited = 0;
	group.applyOnMembers([&visited](model::gameobjects::AionObject&) {
		++visited;
		return visited < 2;
	});
	EXPECT_EQ(visited, 2);
}

/** TemporaryPlayerTeam.getOnlineMembers = filterMembers(ONLINE): an offline member is left out, the members list keeps him */
TEST_F(TeamCoreTest, GetOnlineMembersLeavesOutAnOfflineMember) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	c.player().setClientConnection(nullptr);
	std::vector<runtime::Ptr<Player>> online = group.getOnlineMembers();
	EXPECT_EQ(online.size(), 2u);
	for (const runtime::Ptr<Player>& p : online)
		EXPECT_NE(p.rawPointer(), &c.player());
	EXPECT_EQ(group.getMembers().size(), 3u);
	c.client->enterWorld(c.f);
}

/**
 * TeamKinahDistributionEvent.java:30-50: amount / onlineMembers to each online member including the giver, no range check; the giver pays the
 * whole amount and the remainder vanishes (100 over three: 33 each, one kinah lost).
 */
TEST_F(TeamCoreTest, TheKinahSplitPaysEveryOnlineMemberIncludingTheGiverAndLosesTheRemainder) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	giveKinah(a, 730001, 1000);
	giveKinah(b, 730002, 10);
	giveKinah(c, 730003, 0);
	clearAll();
	PlayerGroupService::distributeKinah(a.player(), 100);
	EXPECT_EQ(a.player().getInventory().getKinah(), 1000 - 100 + 33);
	EXPECT_EQ(b.player().getInventory().getKinah(), 10 + 33);
	EXPECT_EQ(c.player().getInventory().getKinah(), 33);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_ME_TO_B(100, 3, 33)), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_B_TO_ME("Alpha", 100, 3, 33)), 1);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_B_TO_ME("Alpha", 100, 3, 33)), 1);
	static_cast<void>(group);
}

TEST_F(TeamCoreTest, TheKinahSplitRefusesTooLittleMoneyAndSkipsOfflineMembers) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	form({&a, &b, &c});
	giveKinah(a, 730001, 50);
	giveKinah(b, 730002, 0);
	giveKinah(c, 730003, 0);
	PlayerGroupService::distributeKinah(a.player(), 100);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY()), 1);
	EXPECT_EQ(a.player().getInventory().getKinah(), 50);
	c.player().setClientConnection(nullptr);
	PlayerGroupService::distributeKinah(a.player(), 50); // two online: 25 each
	EXPECT_EQ(a.player().getInventory().getKinah(), 25);
	EXPECT_EQ(b.player().getInventory().getKinah(), 25);
	EXPECT_EQ(c.player().getInventory().getKinah(), 0);
	c.client->enterWorld(c.f);
}

/** TeamCommand.getCodeId / getCommand (TeamCommand.java:10-56): every code round-trips, an unknown one is requireNonNull's NPE */
TEST(TeamCommandTest, CodesRoundTripAndAnUnknownCodeThrows) {
	const int32_t codes[] = {2, 3, 6, 9, 10, 11, 14, 16, 17, 20, 21, 22, 23, 24, 25, 26, 27, 29, 30, 31, 32};
	for (int32_t code : codes)
		EXPECT_EQ(model::team::common::events::getCodeId(model::team::common::events::getCommand(code)), code);
	EXPECT_EQ(model::team::common::events::getCommand(6), TeamCommand::GROUP_REMOVE_MEMBER);
	EXPECT_THROW(model::team::common::events::getCommand(7), runtime::NullPointerException);
}

/** LootGroupRules.java:32-80: the defaults (ROUNDROBIN, common 0, superior..mythic 2), the quality rule, the misc rule, the distribution id */
TEST(LootGroupRulesTest, DefaultsQualityRulesAndDistributionIds) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<LootGroupRules> rules = LootGroupRules::create();
	EXPECT_EQ(rules->getLootRule(), LootRuleType::ROUNDROBIN);
	EXPECT_FALSE(rules->getQualityRule(ItemQuality::JUNK));
	EXPECT_FALSE(rules->getQualityRule(ItemQuality::COMMON));
	for (ItemQuality q : {ItemQuality::RARE, ItemQuality::LEGEND, ItemQuality::UNIQUE, ItemQuality::EPIC, ItemQuality::MYTHIC})
		EXPECT_TRUE(rules->getQualityRule(q));
	EXPECT_EQ(rules->getAutodistributionId(), 2);
	EXPECT_FALSE(rules->isMisc(ItemQuality::JUNK));
	runtime::Ref<LootGroupRules> bid = LootGroupRules::create(LootRuleType::FREEFORALL, 1, 1, 0, 0, 0, 0, 3);
	EXPECT_EQ(bid->getAutodistributionId(), 3);
	EXPECT_TRUE(bid->isMisc(ItemQuality::JUNK));
	EXPECT_FALSE(bid->isMisc(ItemQuality::COMMON));
	EXPECT_TRUE(bid->getQualityRule(ItemQuality::COMMON));
	EXPECT_FALSE(bid->getQualityRule(ItemQuality::RARE));
	runtime::Ref<LootGroupRules> none = LootGroupRules::create(LootRuleType::LEADER, 0, 0, 0, 0, 0, 0, 0);
	EXPECT_EQ(none->getAutodistributionId(), 0);
}

/** PlayerTeamCommandService.java:22-27 and :80-89: no team is a no-op, member 0 is the sender, an unknown member is a NullPointerException */
TEST_F(TeamCoreTest, TeamCommandsFindTheirMemberOrThrow) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	using model::team::common::service::PlayerTeamCommandService;
	PlayerTeamCommandService::executeCommand(a.player(), TeamCommand::GROUP_REMOVE_MEMBER, 0); // solo: nothing
	PlayerGroup& group = form({&a, &b, &c});
	EXPECT_THROW(PlayerTeamCommandService::executeCommand(a.player(), TeamCommand::GROUP_BAN_MEMBER, 4711), runtime::NullPointerException);
	PlayerTeamCommandService::executeCommand(a.player(), TeamCommand::GROUP_SET_LEADER, b.player().getObjectId());
	EXPECT_TRUE(group.isLeader(b.player()));
	PlayerTeamCommandService::executeCommand(c.player(), TeamCommand::GROUP_REMOVE_MEMBER, 0);
	EXPECT_FALSE(c.player().getPlayerGroup());
	EXPECT_EQ(group.size(), 2);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
