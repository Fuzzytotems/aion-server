// P5-10c alliances (m5g-plan.md AL-02, AL-03, AL-05; §16.3 items 3-4): PlayerAllianceService and the 12 alliance events driven in process
// on the party fixture of tests/team/P5-10b - the invite (solo, the group conversion of both sides, the redirect to a group's leader), the
// leave / ban / disband paths with the disband breaker, the leader change through a vice captain, the vice-captain limits, the group moves,
// the ready check, the disconnect / reconnect pair, the offline checker, the update and kinah events.
//
// Expectations are derived by hand from PlayerAllianceService.java:42-283 and alliance/events/*.java. Every alliance a test forms is disbanded
// by TearDown; ctest runs every case in a process of its own (the registry and the offline checker are static).

#include "../P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent.h"
#include "aion/gameserver/model/team/common/events/TeamCommand.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_READY_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::team::alliance::PlayerAlliance;
using model::team::alliance::PlayerAllianceMember;
using model::team::alliance::PlayerAllianceService;
using model::team::alliance::events::AssignViceCaptainEvent_AssignType;
using model::team::common::events::TeamCommand;
using model::team::common::legacy::PlayerAllianceEvent;
using serverpackets::SM_ALLIANCE_INFO;
using serverpackets::SM_ALLIANCE_MEMBER_INFO;
using serverpackets::SM_ALLIANCE_READY_CHECK;
using serverpackets::SM_QUESTION_WINDOW;

constexpr int32_t QUESTION = SM_QUESTION_WINDOW::STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION;

class AllianceLifecycleTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		runtime::LockOrderValidator::getInstance().clearReports();
	}

	void TearDown() override {
		// m5g-1: no alliance path leaves a lock-order report (same-class nesting of the alliance and its groups, or a cycle)
		for (const runtime::LockOrderValidator::Report& report : runtime::LockOrderValidator::getInstance().getReports())
			ADD_FAILURE() << report.text;
		for (Member& m : members) {
			if (runtime::Ptr<PlayerAlliance> alliance = m.player().getPlayerAlliance())
				PlayerAllianceService::disband(*alliance, false);
		}
		TeamTest::TearDown();
	}

	/** inviter invites invited, who answers yes (PlayerAllianceInvite.acceptRequest) */
	void invite(Member& inviter, Member& invited) {
		PlayerAllianceService::inviteToAlliance(inviter.player(), invited.player());
		ASSERT_TRUE(invited.player().getResponseRequester().respond(QUESTION, 1)) << "no alliance question for " << invited.player().getName();
	}

	/** an alliance of the given members, the first as leader, each invited by the leader */
	PlayerAlliance& alliance(std::initializer_list<Member*> list) {
		auto it = list.begin();
		Member& leader = **it++;
		for (; it != list.end(); ++it)
			invite(leader, **it);
		clearAll();
		return *leader.player().getPlayerAlliance();
	}

	int32_t groupOf(Member& m) { return m.player().getPlayerAllianceGroup()->getTeamId(); }

	static constexpr int32_t SM_ALLIANCE_INFO_OPCODE = opcodeOf<SM_ALLIANCE_INFO>;
	static constexpr int32_t SM_ALLIANCE_MEMBER_INFO_OPCODE = opcodeOf<SM_ALLIANCE_MEMBER_INFO>;
};

/** inviteToAlliance (PlayerAllianceService.java:42-62) to a solo player, then createAlliance (:64-73) and two PlayerAllianceEnteredEvents */
TEST_F(AllianceLifecycleTest, ASoloInviteCreatesTheAlliance) {
	Member& a = addMember("Alpha");
	Member& c = addMember("Charlie");
	PlayerAllianceService::inviteToAlliance(a.player(), c.player());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_INVITED_HIM("Charlie")), 1);
	EXPECT_EQ(c.count(SM_QUESTION_WINDOW(QUESTION, 0, 0, "Alpha")), 1);
	ASSERT_TRUE(c.player().getResponseRequester().respond(QUESTION, 1));
	runtime::Ptr<PlayerAlliance> alliance = a.player().getPlayerAlliance();
	ASSERT_TRUE(alliance);
	EXPECT_EQ(c.player().getPlayerAlliance().get(), alliance.get());
	EXPECT_EQ(alliance->size(), 2);
	EXPECT_TRUE(alliance->isLeader(a.player()));
	EXPECT_EQ(groupOf(a), 1000);
	EXPECT_EQ(groupOf(c), 1000);
	EXPECT_EQ(PlayerAllianceService::searchAlliance(c.player().getObjectId()).get(), alliance.get());
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_FORCE_ENTERED_FORCE()), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_HE_ENTERED_FORCE("Charlie")), 1);
	EXPECT_GE(c.count(SM_ALLIANCE_INFO_OPCODE), 1);
	EXPECT_GE(c.count(SM_ALLIANCE_MEMBER_INFO_OPCODE), 2) << "its own JOIN and A's ENTER";
}

/** denyRequest: STR_PARTY_ALLIANCE_HE_REJECT_INVITATION (PlayerAllianceInvite.java:69-72) */
TEST_F(AllianceLifecycleTest, ADeclinedInviteTellsTheInviter) {
	Member& a = addMember("Alpha");
	Member& c = addMember("Charlie");
	PlayerAllianceService::inviteToAlliance(a.player(), c.player());
	ASSERT_TRUE(c.player().getResponseRequester().respond(QUESTION, 0));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_HE_REJECT_INVITATION("Charlie")), 1);
	EXPECT_FALSE(a.player().getPlayerAlliance());
}

/** GA1: the inviter's group without him and the invited (solo) join; the group is dissolved first (PlayerAllianceInvite.java:28-66) */
TEST_F(AllianceLifecycleTest, TheInvitersGroupBecomesTheAlliance) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	form({&a, &b});
	invite(a, c);
	EXPECT_FALSE(a.player().getPlayerGroup());
	EXPECT_FALSE(b.player().getPlayerGroup());
	runtime::Ptr<PlayerAlliance> alliance = a.player().getPlayerAlliance();
	ASSERT_TRUE(alliance);
	EXPECT_EQ(alliance->size(), 3);
	for (Member* m : {&a, &b, &c}) {
		EXPECT_EQ(m->player().getPlayerAlliance().get(), alliance.get()) << m->player().getName();
		EXPECT_EQ(groupOf(*m), 1000) << m->player().getName();
	}
	EXPECT_TRUE(alliance->isLeader(a.player()));
	EXPECT_GE(b.count(SM_LEAVE_GROUP_MEMBER_OPCODE), 1) << "the group dissolved";
}

/** the invited player's whole group joins; a member who is not his group's leader redirects the invite to the leader
 * (PlayerAllianceService.java:45-56, PlayerAllianceInvite.java:53-60) */
TEST_F(AllianceLifecycleTest, AnInviteToAGroupMemberGoesToItsLeader) {
	Member& a = addMember("Alpha");
	Member& d = addMember("Delta");
	Member& e = addMember("Echo");
	form({&d, &e});
	PlayerAllianceService::inviteToAlliance(a.player(), e.player());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_INVITE_PARTY_HIM("Echo", "Delta")), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_INVITE_PARTY("Delta", 2)), 1);
	EXPECT_EQ(e.count(SM_QUESTION_WINDOW_OPCODE), 0);
	ASSERT_TRUE(d.player().getResponseRequester().respond(QUESTION, 1));
	runtime::Ptr<PlayerAlliance> alliance = a.player().getPlayerAlliance();
	ASSERT_TRUE(alliance);
	EXPECT_EQ(alliance->size(), 3);
	EXPECT_FALSE(d.player().getPlayerGroup());
	EXPECT_EQ(e.player().getPlayerAlliance().get(), alliance.get());
}

/** PlayerAllianceLeavedEvent LEAVE (:37-80): the others get STR_FORCE_LEAVE_HIM, LEAVE info and SM_ALLIANCE_INFO */
TEST_F(AllianceLifecycleTest, AMemberLeavesAnAllianceOfThree) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::removePlayer(c.player());
	EXPECT_EQ(value.size(), 2);
	EXPECT_FALSE(c.player().getPlayerAlliance());
	EXPECT_FALSE(c.player().getPlayerAllianceGroup());
	for (Member* m : {&a, &b}) {
		EXPECT_EQ(m->count(SM_SYSTEM_MESSAGE::STR_FORCE_LEAVE_HIM("Charlie")), 1);
		EXPECT_EQ(m->count(SM_ALLIANCE_INFO_OPCODE), 1);
		EXPECT_EQ(m->count(SM_ALLIANCE_MEMBER_INFO_OPCODE), 1);
	}
	EXPECT_TRUE(c.sent().empty() || c.count(SM_SYSTEM_MESSAGE::STR_FORCE_LEAVE_HIM("Charlie")) == 0);
}

/** a leave from two members disbands (shouldDisband, disband(team, true)): DISBAND for the rest, the four groups released (cycles.toml
 * PlayerAlliance.groups), the registry emptied */
TEST_F(AllianceLifecycleTest, ALeaveFromTwoDisbandsAndReleasesTheGroups) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	runtime::Ref<PlayerAlliance> value(alliance({&a, &b}));
	runtime::Ref<model::team::alliance::PlayerAllianceGroup> group(*a.player().getPlayerAllianceGroup());
	PlayerAllianceService::removePlayer(b.player());
	EXPECT_FALSE(a.player().getPlayerAlliance());
	EXPECT_EQ(value->size(), 0);
	EXPECT_EQ(value->groupSize(), 0) << "the disband breaker";
	EXPECT_EQ(group->size(), 0);
	EXPECT_FALSE(PlayerAllianceService::searchAlliance(a.player().getObjectId()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_LEAVE_HIM("Bravo")), 1) << "B's LEAVE, to A before the disband";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_DISPERSED()), 1) << "the nested DISBAND leave: to the leaver only (nobody is left)";
}

/** banPlayer (:142-163): self, a non-leader, then the leader's ban */
TEST_F(AllianceLifecycleTest, OnlyTheLeaderBansAndNotHimself) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::banPlayer(a.player(), a.player());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_CANT_BAN_SELF()), 1);
	PlayerAllianceService::banPlayer(c.player(), b.player());
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_FORCE_ONLY_LEADER_CAN_BANISH()), 1);
	EXPECT_EQ(value.size(), 3);
	PlayerAllianceService::banPlayer(c.player(), a.player());
	EXPECT_EQ(value.size(), 2);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_FORCE_BAN_ME("Alpha")), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_FORCE_BAN_HIM("Alpha", "Charlie")), 1);
}

/** ChangeAllianceLeaderEvent without an event player (:30-40): an online vice captain first */
TEST_F(AllianceLifecycleTest, ALeavingLeaderPassesLeadershipToAViceCaptain) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::changeViceCaptain(c.player(), AssignViceCaptainEvent_AssignType::PROMOTE);
	EXPECT_TRUE(value.isViceCaptain(c.player()));
	clearAll();
	PlayerAllianceService::removePlayer(a.player());
	EXPECT_TRUE(value.isLeader(c.player()));
	EXPECT_FALSE(value.isViceCaptain(c.player())) << "changeLeaderTo removes the new leader from the vice captains";
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_FORCE_YOU_BECOME_NEW_LEADER()), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_FORCE_HE_IS_NEW_LEADER("Charlie")), 0) << "eventPlayer null: no announcement";
}

/** changeLeader with an event player (:41-45): the old leader becomes a vice captain (DEMOTE_CAPTAIN_TO_VICECAPTAIN) */
TEST_F(AllianceLifecycleTest, AnAppointedLeaderDemotesTheOldOneToViceCaptain) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::changeLeader(b.player());
	EXPECT_TRUE(value.isLeader(b.player()));
	EXPECT_TRUE(value.isViceCaptain(a.player()));
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_FORCE_HE_IS_NEW_LEADER("Bravo")), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_FORCE_YOU_BECOME_NEW_LEADER()), 1);
}

/** AssignViceCaptainEvent (:34-68): four vice captains at most, PROMOTE / DEMOTE message ids in SM_ALLIANCE_INFO */
TEST_F(AllianceLifecycleTest, AtMostFourViceCaptains) {
	std::vector<Member*> list;
	for (int32_t i = 0; i < 6; i++)
		list.push_back(&addMember("Ally" + std::to_string(i + 1), 100.0f + static_cast<float>(i)));
	PlayerAlliance& value = alliance({list[0], list[1], list[2], list[3], list[4], list[5]});
	for (int32_t i = 1; i <= 4; i++)
		PlayerAllianceService::changeViceCaptain(list[i]->player(), AssignViceCaptainEvent_AssignType::PROMOTE);
	EXPECT_EQ(value.getViceCaptainIds().size(), 4);
	EXPECT_EQ(list[0]->count(SM_ALLIANCE_INFO_OPCODE), 4) << "one SM_ALLIANCE_INFO(team, VICECAPTAIN_PROMOTE, name) per promotion";
	clearAll();
	PlayerAllianceService::changeViceCaptain(list[5]->player(), AssignViceCaptainEvent_AssignType::PROMOTE);
	EXPECT_EQ(value.getViceCaptainIds().size(), 4);
	EXPECT_EQ(list[0]->count(SM_SYSTEM_MESSAGE::STR_FORCE_CANNOT_PROMOTE_MANAGER()), 1);
	EXPECT_EQ(list[5]->count(SM_ALLIANCE_INFO_OPCODE), 0);
	PlayerAllianceService::changeViceCaptain(list[1]->player(), AssignViceCaptainEvent_AssignType::DEMOTE);
	EXPECT_FALSE(value.isViceCaptain(list[1]->player()));
}

/** changeMemberGroup (:200-212) and ChangeMemberGroupEvent: a vice captain or the leader moves and swaps; a plain member is refused */
TEST_F(AllianceLifecycleTest, CaptainsMoveAndSwapMembersBetweenGroups) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::changeMemberGroup(c.player(), b.player().getObjectId(), 0, 1001);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_FORCE_RIGHT_NOT_HAVE()), 1);
	EXPECT_EQ(groupOf(b), 1000);
	PlayerAllianceService::changeMemberGroup(a.player(), b.player().getObjectId(), 0, 1001);
	EXPECT_EQ(groupOf(b), 1001);
	EXPECT_EQ(value.getMember(b.player().getObjectId())->getAllianceId(), 1001);
	EXPECT_EQ(value.getAllianceGroup(1000)->size(), 2);
	EXPECT_EQ(c.count(SM_ALLIANCE_MEMBER_INFO(*value.getMember(b.player().getObjectId()), PlayerAllianceEvent::MEMBER_GROUP_CHANGE)), 1);
	PlayerAllianceService::changeMemberGroup(a.player(), b.player().getObjectId(), c.player().getObjectId(), 0);
	EXPECT_EQ(groupOf(b), 1000);
	EXPECT_EQ(groupOf(c), 1001);
	Member& d = addMember("Delta", 130.0f);
	PlayerAllianceService::changeMemberGroup(d.player(), 0, 0, 1001);
	EXPECT_EQ(d.count(SM_SYSTEM_MESSAGE::STR_FORCE_YOU_ARE_NOT_FORCE_MEMBER()), 1);
}

/** CheckAllianceReadyEvent (:27-70): START counts the other online members, READY and NOT_READY count down, the last answer sends (0, 3) */
TEST_F(AllianceLifecycleTest, TheReadyCheckCountsDown) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::checkReady(a.player(), TeamCommand::ALLIANCE_CHECKREADY_START);
	EXPECT_EQ(value.getAllianceReadyStatus(), 2);
	for (Member* m : {&a, &b, &c}) {
		EXPECT_EQ(m->count(SM_ALLIANCE_READY_CHECK(a.player().getObjectId(), 5)), 1);
		EXPECT_EQ(m->count(SM_ALLIANCE_READY_CHECK(a.player().getObjectId(), 1)), 1);
	}
	PlayerAllianceService::checkReady(b.player(), TeamCommand::ALLIANCE_CHECKREADY_READY);
	EXPECT_EQ(value.getAllianceReadyStatus(), 1);
	EXPECT_EQ(a.count(SM_ALLIANCE_READY_CHECK(0, 3)), 0);
	PlayerAllianceService::checkReady(c.player(), TeamCommand::ALLIANCE_CHECKREADY_NOTREADY);
	EXPECT_EQ(value.getAllianceReadyStatus(), 0);
	EXPECT_EQ(a.count(SM_ALLIANCE_READY_CHECK(c.player().getObjectId(), 4)), 1);
	EXPECT_EQ(a.count(SM_ALLIANCE_READY_CHECK(0, 3)), 1);
	PlayerAllianceService::checkReady(a.player(), TeamCommand::ALLIANCE_CHECKREADY_CANCEL);
	EXPECT_EQ(b.count(SM_ALLIANCE_READY_CHECK(a.player().getObjectId(), 0)), 1);
}

/** onPlayerLogout / PlayerDisconnectedEvent (:28-61): the leader's disconnect passes the lead; the last online member's disbands */
TEST_F(AllianceLifecycleTest, DisconnectsPassTheLeadAndTheLastOneDisbands) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	runtime::Ref<PlayerAlliance> value(alliance({&a, &b}));
	a.player().setClientConnection(nullptr); // PlayerLeaveWorldService sets the connection null first
	PlayerAllianceService::onPlayerLogout(a.player());
	EXPECT_TRUE(value->isLeader(b.player()));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_FORCE_HE_BECOME_OFFLINE("Alpha")), 1);
	EXPECT_GT(value->getMember(a.player().getObjectId())->getLastOnlineTime(), 0);
	EXPECT_EQ(value->size(), 2) << "an offline member stays";
	b.player().setClientConnection(nullptr);
	PlayerAllianceService::onPlayerLogout(b.player());
	EXPECT_EQ(value->size(), 0) << "no online member: disband(alliance, false)";
	EXPECT_EQ(value->groupSize(), 0);
	EXPECT_FALSE(a.player().getPlayerAlliance());
	a.client->enterWorld(a.f); // TearDown resets the connections of online players
	b.client->enterWorld(b.f);
}

/** onPlayerLogin / PlayerConnectedEvent (:26-41): the member is replaced by a new one for the same player, RECONNECT to everyone */
TEST_F(AllianceLifecycleTest, AReconnectReplacesTheMember) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	runtime::Ptr<PlayerAllianceMember> before = value.getMember(c.player().getObjectId());
	c.player().setClientConnection(nullptr);
	PlayerAllianceService::onPlayerLogout(c.player());
	c.client->enterWorld(c.f); // the connection again
	clearAll();
	PlayerAllianceService::onPlayerLogin(c.player());
	runtime::Ptr<PlayerAllianceMember> after = value.getMember(c.player().getObjectId());
	ASSERT_TRUE(after);
	EXPECT_NE(after.get(), before.get());
	EXPECT_EQ(value.size(), 3);
	EXPECT_EQ(a.count(SM_ALLIANCE_MEMBER_INFO(*after, PlayerAllianceEvent::RECONNECT)), 1);
	EXPECT_EQ(c.count(SM_ALLIANCE_INFO_OPCODE), 1);
	EXPECT_EQ(c.count(SM_ALLIANCE_MEMBER_INFO_OPCODE), 3) << "its own RECONNECT and the two others'";
	Member& d = addMember("Delta", 130.0f);
	PlayerAllianceService::onPlayerLogin(d.player()); // a solo login passes the registry (E-12)
	EXPECT_FALSE(d.player().getPlayerAlliance());
}

/** OfflinePlayerAllianceChecker (:265-281): an offline member past ALLIANCE_REMOVE_TIME leaves with LEAVE_TIMEOUT */
TEST_F(AllianceLifecycleTest, TheOfflineCheckerRemovesAnExpiredMember) {
	// removetime -1: lastOnline - 1000 is already expired (TimeUtil.isExpired reads the wall clock)
	ConfigScope<int32_t> removeTime(configs::main::GroupConfig::ALLIANCE_REMOVE_TIME, -1);
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerAlliance& value = alliance({&a, &b, &c});
	c.player().setClientConnection(nullptr);
	PlayerAllianceService::onPlayerLogout(c.player());
	executor->advance(std::chrono::milliseconds(1000)); // the checker's first run (initial delay 1000 ms)
	EXPECT_FALSE(value.hasMember(c.player().getObjectId()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_HE_LEAVED_PARTY_OFFLINE_TIMEOUT("Charlie")), 1);
	EXPECT_EQ(value.size(), 2);
	c.client->enterWorld(c.f); // TearDown resets the connections of online players
}

/** updateAlliance / updateAllianceEffects (PlayerAllianceUpdateEvent:38-48): to every member but the player */
TEST_F(AllianceLifecycleTest, UpdatesGoToTheOtherMembers) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerAlliance& value = alliance({&a, &b});
	PlayerAllianceService::updateAlliance(a.player(), PlayerAllianceEvent::MOVEMENT);
	EXPECT_EQ(b.count(SM_ALLIANCE_MEMBER_INFO(*value.getMember(a.player().getObjectId()), PlayerAllianceEvent::MOVEMENT)), 1);
	EXPECT_EQ(a.count(SM_ALLIANCE_MEMBER_INFO_OPCODE), 0);
	PlayerAllianceService::updateAllianceEffects(a.player(), 1);
	EXPECT_EQ(b.count(SM_ALLIANCE_MEMBER_INFO(*value.getMember(a.player().getObjectId()), PlayerAllianceEvent::UPDATE_EFFECTS, 1)), 1);
	PlayerAllianceService::updateAlliance(a.player(), PlayerAllianceEvent::JOIN);
	EXPECT_EQ(b.count(SM_ALLIANCE_MEMBER_INFO_OPCODE), 2) << "JOIN is unsupported: nothing sent";
}

/** distributeKinah over the alliance, distributeKinahInGroup over the alliance group (:236-250, TeamKinahDistributionEvent) */
TEST_F(AllianceLifecycleTest, KinahIsSplitOverTheAllianceOrTheGroup) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	giveKinah(a, 739201, 1000);
	giveKinah(b, 739202, 0);
	giveKinah(c, 739203, 0);
	PlayerAlliance& value = alliance({&a, &b, &c});
	PlayerAllianceService::changeMemberGroup(a.player(), c.player().getObjectId(), 0, 1001);
	PlayerAllianceService::distributeKinah(a.player(), 300);
	EXPECT_EQ(a.player().getInventory().getKinah(), 1000 - 300 + 100);
	EXPECT_EQ(c.player().getInventory().getKinah(), 100);
	PlayerAllianceService::distributeKinahInGroup(a.player(), 100);
	EXPECT_EQ(b.player().getInventory().getKinah(), 100 + 50);
	EXPECT_EQ(c.player().getInventory().getKinah(), 100) << "C is in group 1001";
	EXPECT_EQ(value.size(), 3);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
