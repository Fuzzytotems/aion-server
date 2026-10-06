// P5-10d leagues (m5g-plan.md LG-01..LG-04): League, LeagueService and the nine league events driven in process on the party fixture of
// tests/team/P5-10b, with two alliances (A, B) and (C, D):
// - create / join through the invite (the redirect to the invited alliance's leader), the league loot rules read through the alliance,
//   move, the leader change, the kinah split over the league, leave and expel with reorganize and the disband of a league of one, and
//   the expel by a player who is not the league leader (IllegalArgumentException, LeagueService.java:114-122);
// - LG-04's lock-order test: every league event and the alliance-side sites of D4 (a vice captain's promotion, a leader change, a member
//   entering, leaving and disconnecting, an alliance disband in a league) run on one thread with the validator on; no report may appear.
//   The league events record League -> PlayerAlliance; each alliance-side site takes the league under its alliance inside a LockdepSuppression
//   (D4(c)) - remove one and the validator reports a CYCLE (checked by hand, m5g-plan.md §16).
//
// Expectations are derived by hand from League.java, LeagueService.java and league/events/*.java.

#include "../P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::team::alliance::PlayerAlliance;
using model::team::alliance::PlayerAllianceService;
using model::team::league::League;
using model::team::league::LeagueMember;
using model::team::league::LeagueService;
using serverpackets::SM_ALLIANCE_INFO;
using serverpackets::SM_QUESTION_WINDOW;

constexpr int32_t ALLIANCE_QUESTION = SM_QUESTION_WINDOW::STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION;
constexpr int32_t LEAGUE_QUESTION = SM_QUESTION_WINDOW::STR_MSGBOX_UNION_INVITE_ME;

class LeagueLifecycleTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		runtime::LockOrderValidator::getInstance().clearReports(true);
	}

	void TearDown() override {
		for (const runtime::LockOrderValidator::Report& report : runtime::LockOrderValidator::getInstance().getReports())
			ADD_FAILURE() << report.text;
		for (Member& m : members) {
			if (runtime::Ptr<PlayerAlliance> alliance = m.player().getPlayerAlliance()) {
				if (runtime::Ptr<League> league = alliance->getLeague())
					LeagueService::disband(*league);
			}
		}
		for (Member& m : members) {
			if (runtime::Ptr<PlayerAlliance> alliance = m.player().getPlayerAlliance())
				PlayerAllianceService::disband(*alliance, false);
		}
		TeamTest::TearDown();
	}

	/** leader invites member to an alliance (PlayerAllianceService.inviteToAlliance) and member answers yes */
	PlayerAlliance& alliance(Member& leader, Member& member) {
		PlayerAllianceService::inviteToAlliance(leader.player(), member.player());
		EXPECT_TRUE(member.player().getResponseRequester().respond(ALLIANCE_QUESTION, 1));
		return *leader.player().getPlayerAlliance();
	}

	/** two alliances (A, B) and (C, D), A's alliance leads the league C's alliance joined */
	League& league() {
		a = &addMember("Alpha");
		b = &addMember("Bravo");
		c = &addMember("Charlie");
		d = &addMember("Delta");
		first = &alliance(*a, *b);
		second = &alliance(*c, *d);
		LeagueService::inviteToLeague(a->player(), d->player()); // D is no leader: the invite goes to C
		EXPECT_TRUE(c->player().getResponseRequester().respond(LEAGUE_QUESTION, 1)) << "the league question went to C";
		clearAll();
		return *first->getLeague();
	}

	Member* a = nullptr;
	Member* b = nullptr;
	Member* c = nullptr;
	Member* d = nullptr;
	PlayerAlliance* first = nullptr;
	PlayerAlliance* second = nullptr;
};

/** inviteToLeague (LeagueService.java:30-49), LeagueInviteEvent, createLeague (:84-93) and LeagueJoinEvent */
TEST_F(LeagueLifecycleTest, AnInviteCreatesTheLeagueAndTheSecondAllianceJoins) {
	League& value = league();
	EXPECT_EQ(value.size(), 2);
	EXPECT_EQ(second->getLeague().get(), &value);
	EXPECT_TRUE(value.isLeader(*first));
	EXPECT_EQ(value.getCaptain().get(), &a->player());
	EXPECT_EQ(value.getMember(first->getObjectId())->getLeaguePosition(), 0);
	EXPECT_EQ(value.getMember(second->getObjectId())->getLeaguePosition(), 1);
	EXPECT_EQ(value.getOnlineMembers().size(), 4u);
	EXPECT_EQ(value.getPlayerMember(d->player().getObjectId()).get(), &d->player());
	EXPECT_EQ(value.getCaptains().size(), 2u);
	EXPECT_EQ(LeagueService::getLeagues().size(), 1u);
	// the league's rules, FREEFORALL 0 0 2 2 2 2 2 (LeagueService.java:88), are what the alliances read
	runtime::Ptr<model::team::common::legacy::LootGroupRules> rules = first->getLootGroupRules();
	EXPECT_EQ(rules.get(), value.getLootGroupRules().get());
	EXPECT_EQ(rules->getLootRule(), model::team::common::legacy::LootRuleType::FREEFORALL);
	EXPECT_EQ(rules->getCommonItemAbove(), 0);
	EXPECT_EQ(rules->getSuperiorItemAbove(), 2);
}

/** canInvite (LeagueService.java:51-82): a player without an alliance, an own alliance member, an alliance already in the league */
TEST_F(LeagueLifecycleTest, TheInviteRefusals) {
	League& value = league();
	Member& e = addMember("Echo", 120.0f);
	EXPECT_FALSE(LeagueService::canInvite(a->player(), e.player()));
	EXPECT_EQ(a->count(SM_SYSTEM_MESSAGE::STR_UNION_CANT_INVITE_WHEN_HE_IS_ASKED_QUESTION("Echo")), 1);
	EXPECT_FALSE(LeagueService::canInvite(a->player(), b->player()));
	EXPECT_EQ(a->count(SM_SYSTEM_MESSAGE::STR_UNION_CANT_INVITE_SELF()), 1);
	EXPECT_FALSE(LeagueService::canInvite(a->player(), c->player()));
	EXPECT_EQ(a->count(SM_SYSTEM_MESSAGE::STR_UNION_ALREADY_MY_UNION()), 1);
	EXPECT_EQ(value.size(), 2);
}

/** moveAlliance by the league leader (LeagueService.java:144-149) and LeagueMoveEvent: the positions swap */
TEST_F(LeagueLifecycleTest, TheLeaderMovesTheAlliances) {
	League& value = league();
	LeagueService::moveAlliance(c->player(), first->getObjectId(), second->getObjectId()); // not the league leader: nothing
	EXPECT_EQ(value.getMember(first->getObjectId())->getLeaguePosition(), 0);
	LeagueService::moveAlliance(a->player(), first->getObjectId(), second->getObjectId());
	EXPECT_EQ(value.getMember(first->getObjectId())->getLeaguePosition(), 1);
	EXPECT_EQ(value.getMember(second->getObjectId())->getLeaguePosition(), 0);
	EXPECT_EQ(a->count(SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_ME(1)), 1);
	EXPECT_EQ(c->count(SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_ME(0)), 1);
	// back: a league whose leader's alliance is not at position 0 makes a later reorganize call changeLeader on the leader, which throws in Java
	// too (a proposed correction, docs/deviations/P5-10d.md); TearDown's disband would meet it in one of the two removal orders
	LeagueService::moveAlliance(a->player(), first->getObjectId(), second->getObjectId());
	EXPECT_EQ(value.getMember(first->getObjectId())->getLeaguePosition(), 0);
}

/**
 * setLeader / LeagueChangeLeaderEvent (:30-52): C, his alliance's leader, takes the league lead. Java's position swap reads
 * `league.getMember(team.getObjectId())`, and `team` is C's own alliance, so C's alliance gets its old position back and the old leader's keeps
 * 0: no swap (a proposed correction, docs/deviations/P5-10d.md). The port does what Java does.
 */
TEST_F(LeagueLifecycleTest, AnAllianceLeaderTakesTheLeagueLead) {
	League& value = league();
	LeagueService::setLeader(c->player(), c->player());
	EXPECT_TRUE(value.isLeader(*second));
	EXPECT_EQ(value.getMember(second->getObjectId())->getLeaguePosition(), 1) << "Java: the new leader's alliance keeps its position";
	EXPECT_EQ(value.getMember(first->getObjectId())->getLeaguePosition(), 0);
	EXPECT_EQ(value.getCaptain().get(), &c->player());
	EXPECT_EQ(d->count(SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_FORCE_NUMBER_ME(1)), 1) << "D's alliance was at position 1";
	EXPECT_EQ(c->count(SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_LEADER("Charlie", "Charlie")), 1) << "to the alliance leader";
	LeagueService::setLeader(d->player(), d->player()); // D leads no alliance: nothing
	EXPECT_TRUE(value.isLeader(*second));
}

/** changeGroupRules and LeagueLootRulesChangeEvent: the alliances read the new rules */
TEST_F(LeagueLifecycleTest, NewLeagueLootRulesReachTheAlliances) {
	League& value = league();
	runtime::Ref<model::team::common::legacy::LootGroupRules> rules =
		model::team::common::legacy::LootGroupRules::create(model::team::common::legacy::LootRuleType::ROUNDROBIN, 0, 0, 0, 0, 0, 0, 0);
	LeagueService::changeGroupRules(value, *rules);
	EXPECT_EQ(second->getLootGroupRules().get(), rules.get());
	EXPECT_GE(d->count(SM_ALLIANCE_INFO(*second)), 1);
}

/** distributeKinah over the league's online members (LeagueKinahDistributionEvent) */
TEST_F(LeagueLifecycleTest, KinahIsSplitOverTheLeague) {
	league();
	giveKinah(*a, 739301, 1000);
	giveKinah(*d, 739302, 0);
	LeagueService::distributeKinah(a->player(), 400);
	EXPECT_EQ(a->player().getInventory().getKinah(), 1000 - 400 + 100);
	EXPECT_EQ(d->player().getInventory().getKinah(), 100);
	EXPECT_EQ(a->count(SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_ME_TO_B(400, 4, 100)), 1);
}

/** removeAlliance (LEAVE) of the league leader: reorganize gives the lead to the other alliance; a league of one disbands */
TEST_F(LeagueLifecycleTest, TheLeadersAllianceLeavesAndTheLeagueOfOneDisbands) {
	runtime::Ref<League> value(league());
	LeagueService::removeAlliance(runtime::Ptr<PlayerAlliance>(*first));
	EXPECT_FALSE(first->isInLeague());
	EXPECT_EQ(a->count(SM_ALLIANCE_INFO(*first, SM_ALLIANCE_INFO::LEAGUE_LEFT_ME, "Alpha")), 1);
	EXPECT_EQ(c->count(SM_SYSTEM_MESSAGE::STR_UNION_CHANGE_LEADER_TIMEOUT("Charlie")), 1) << "reorganize's new leader";
	EXPECT_FALSE(second->isInLeague()) << "shouldDisband: the league of one is disbanded";
	EXPECT_EQ(value->size(), 0);
	EXPECT_TRUE(LeagueService::getLeagues().empty());
}

/** expelAlliance (:114-122): only the league leader's alliance leader expels; anyone else is an IllegalArgumentException */
TEST_F(LeagueLifecycleTest, OnlyTheLeagueLeaderExpels) {
	League& value = league();
	runtime::Ref<LeagueMember> secondMember(*value.getMember(second->getObjectId()));
	runtime::Ref<LeagueMember> firstMember(*value.getMember(first->getObjectId()));
	EXPECT_THROW(LeagueService::expelAlliance(*firstMember, b->player()), commons::utils::IllegalArgumentException) << "B leads no alliance";
	EXPECT_THROW(LeagueService::expelAlliance(*firstMember, c->player()), commons::utils::IllegalArgumentException) << "C's alliance is no league leader";
	LeagueService::expelAlliance(*secondMember, a->player());
	EXPECT_FALSE(second->isInLeague());
	EXPECT_EQ(c->count(SM_ALLIANCE_INFO(*second, SM_ALLIANCE_INFO::LEAGUE_EXPELLED, "Alpha")), 1);
	EXPECT_FALSE(first->isInLeague()) << "the league of one is disbanded";
	EXPECT_EQ(a->count(SM_ALLIANCE_INFO(*first, SM_ALLIANCE_INFO::LEAGUE_DISPERSED, "")), 1);
}

/**
 * LG-04: the league events (join, move, loot rules, leader change, kinah, leave) and D4's alliance-side sites (a vice captain's promotion,
 * an alliance leader change, a member entering, leaving and disconnecting, an alliance disbanding in the league) on one thread: no
 * lock-order report (TearDown fails on any).
 */
TEST_F(LeagueLifecycleTest, TheLeagueAndAllianceLockOrderLeavesNoReport) {
	if (!runtime::LockOrderValidator::getInstance().isEnabled())
		GTEST_SKIP() << "the lock-order validator runs in checked builds only";
	League& value = league();
	Member& e = addMember("Echo", 120.0f);
	Member& f = addMember("Foxtrot", 122.0f);
	// the league side: League -> PlayerAlliance
	LeagueService::moveAlliance(a->player(), first->getObjectId(), second->getObjectId());
	LeagueService::moveAlliance(a->player(), first->getObjectId(), second->getObjectId()); // back (TheLeaderMovesTheAlliances says why)
	LeagueService::changeGroupRules(value, *model::team::common::legacy::LootGroupRules::create());
	giveKinah(*a, 739401, 100);
	LeagueService::distributeKinah(a->player(), 40);
	// the alliance side, each under its alliance's lock: AssignViceCaptainEvent:69, ChangeAllianceLeaderEvent:51 and :63,
	// PlayerAllianceEnteredEvent:46, PlayerAllianceLeavedEvent:59, PlayerDisconnectedEvent:59
	PlayerAllianceService::changeViceCaptain(b->player(), model::team::alliance::events::AssignViceCaptainEvent_AssignType::PROMOTE);
	PlayerAllianceService::changeLeader(b->player());
	PlayerAllianceService::inviteToAlliance(b->player(), e.player());
	EXPECT_TRUE(e.player().getResponseRequester().respond(ALLIANCE_QUESTION, 1));
	PlayerAllianceService::inviteToAlliance(b->player(), f.player());
	EXPECT_TRUE(f.player().getResponseRequester().respond(ALLIANCE_QUESTION, 1));
	PlayerAllianceService::removePlayer(e.player());
	f.player().setClientConnection(nullptr);
	PlayerAllianceService::onPlayerLogout(f.player());
	f.client->enterWorld(f.f);
	EXPECT_TRUE(value.hasMember(first->getObjectId()));
	// LeagueChangeLeaderEvent last on the league side: C's alliance leads (setLeader for the leading alliance itself throws, Java's too)
	LeagueService::setLeader(c->player(), c->player());
	EXPECT_TRUE(value.isLeader(*second));
	// PlayerAllianceService.java:190: an alliance in the league disbands (onBefore: the league is left first)
	PlayerAllianceService::removePlayer(d->player());
	EXPECT_FALSE(second->isInLeague()) << "C's alliance of one disbanded and left the league";
	EXPECT_EQ(runtime::LockOrderValidator::getInstance().reportCount(runtime::LockOrderValidator::ReportKind::CYCLE), 0u);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
