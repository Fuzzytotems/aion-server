// P5-10b parties (m5g-plan.md GR-05): every leave path of §2.4 and the join paths of §2.1 as in-process packet recordings of real Players, the
// leader succession, the offline timeout, the relog replacement, a solo login while a group exists (E-12), disband inside forEach with three and
// six members, the stale PlayerGroupStats reference Java keeps (D7) and the two team updaters.
//
// Expectations are derived by hand from PlayerGroupService.java, the group events (model/team/group/events/*.java), PlayerLeavedEvent.java,
// PlayerEnteredEvent.java, GeneralTeam.java, TemporaryPlayerTeam.java, PlayerGroupStats.java, TeamMoveUpdater.java and TeamStatUpdater.java.

#include "TeamTestSupport.h"

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroupMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEAVE_GROUP_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/taskmanager/tasks/TeamMoveUpdater.h"
#include "aion/gameserver/taskmanager/tasks/TeamStatUpdater.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::team::common::legacy::GroupEvent;
using serverpackets::SM_GROUP_INFO;
using serverpackets::SM_GROUP_MEMBER_INFO;
using serverpackets::SM_LEAVE_GROUP_MEMBER;
using serverpackets::SM_QUESTION_WINDOW;

class PlayerGroupLifecycleTest : public TeamTest {};

/** Java PlayerGroupService.inviteToGroup (:37-45), PlayerGroupInvite.acceptRequest (:22-31), createGroup (:47-58), PlayerGroupEnteredEvent */
TEST_F(PlayerGroupLifecycleTest, InviteAcceptCreatesTheGroupWithTheInviterAsLeader) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	runtime::resetUnportedHitsForTests();

	PlayerGroupService::inviteToGroup(a.player(), b.player());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_INVITED_HIM("Bravo")), 1);
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION, 0, 0, std::string("Alpha"))), 1);
	EXPECT_FALSE(a.player().getPlayerGroup()) << "nothing happens before the answer";

	clearAll();
	EXPECT_TRUE(b.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION, 1));
	runtime::Ptr<PlayerGroup> group = a.player().getPlayerGroup();
	ASSERT_TRUE(group);
	EXPECT_EQ(b.player().getPlayerGroup().rawPointer(), group.rawPointer());
	EXPECT_TRUE(group->isLeader(a.player()));
	EXPECT_EQ(group->size(), 2);
	EXPECT_NE(group->getTeamId(), 0);
	EXPECT_EQ(group->getTeamType(), model::team::TeamType::GROUP);

	// PlayerGroupEnteredEvent.java:24-45 for A (the leader joins first), then for B
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_ENTERED_PARTY()), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_ENTERED_PARTY()), 1);
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(*group, a.player(), GroupEvent::JOIN)), 1);
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO(*group, b.player(), GroupEvent::JOIN)), 1);
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(*group, b.player(), GroupEvent::ENTER)), 1) << "A is told B entered";
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO(*group, a.player(), GroupEvent::ENTER)), 1) << "B is told about A";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_ENTERED_PARTY("Bravo")), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_ENTERED_PARTY("Alpha")), 0) << "the joiner is not told about himself or the earlier members";
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(*group, a.player(), GroupEvent::ENTER)), 0) << "nobody is sent ENTER about himself";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(PlayerGroupLifecycleTest, ADeclinedInviteTellsTheInviterAndCanBeRepeated) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerGroupService::inviteToGroup(a.player(), b.player());
	clearAll();
	EXPECT_TRUE(b.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION, 0));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_REJECT_INVITATION("Bravo")), 1);
	EXPECT_FALSE(a.player().getPlayerGroup());
	EXPECT_EQ(a.count(SM_GROUP_INFO_OPCODE) + b.count(SM_GROUP_INFO_OPCODE), 0);
	// the answered request is gone, so a second invite asks again
	clearAll();
	PlayerGroupService::inviteToGroup(a.player(), b.player());
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW_OPCODE), 1);
}

/** PlayerGroupInvite.acceptRequest's first arm: the inviter's existing group (PlayerGroupInvite.java:25-27) */
TEST_F(PlayerGroupLifecycleTest, AThirdMemberJoinsTheExistingGroup) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b});
	PlayerGroupService::inviteToGroup(a.player(), c.player());
	EXPECT_TRUE(c.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION, 1));
	EXPECT_EQ(c.player().getPlayerGroup().rawPointer(), &group);
	EXPECT_EQ(group.size(), 3);
	EXPECT_EQ(c.count(SM_GROUP_MEMBER_INFO(group, a.player(), GroupEvent::ENTER)), 1);
	EXPECT_EQ(c.count(SM_GROUP_MEMBER_INFO(group, b.player(), GroupEvent::ENTER)), 1);
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(group, c.player(), GroupEvent::ENTER)), 1);
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO(group, c.player(), GroupEvent::ENTER)), 1);
}

/** PlayerGroupLeavedEvent(LEAVE) of a member of three (PlayerGroupLeavedEvent.java:32-56, PlayerLeavedEvent.java:55-69) */
TEST_F(PlayerGroupLifecycleTest, AMemberLeavesAGroupOfThree) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	PlayerGroupService::removePlayer(c.player());
	EXPECT_FALSE(c.player().getPlayerGroup()) << "PlayerGroup.onRemoveMember: setPlayerGroup(null)";
	EXPECT_EQ(group.size(), 2);
	EXPECT_EQ(c.count(SM_LEAVE_GROUP_MEMBER()), 1);
	EXPECT_EQ(c.count(SM_GROUP_MEMBER_INFO_OPCODE), 0) << "the leaver is no member any more when the team is told";
	for (Member* m : {&a, &b}) {
		EXPECT_EQ(m->count(SM_GROUP_MEMBER_INFO(group, c.player(), GroupEvent::LEAVE)), 1);
		EXPECT_EQ(m->count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_LEAVE_PARTY("Charlie")), 1);
	}
	EXPECT_TRUE(group.isLeader(a.player()));
}

/** A leaving leader passes leadership to the next online member (PlayerGroupLeavedEvent.java:52-54, ChangeLeaderEvent.java:23-31) */
TEST_F(PlayerGroupLifecycleTest, ALeavingLeaderPassesLeadershipToAnOnlineMember) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	PlayerGroupService::removePlayer(a.player());
	runtime::Ptr<model::gameobjects::player::Player> leader = group.getLeaderObject();
	ASSERT_TRUE(leader);
	EXPECT_TRUE(leader.rawPointer() == &b.player() || leader.rawPointer() == &c.player()) << "D5: which online member is the C++ map's order";
	Member& newLeader = leader.rawPointer() == &b.player() ? b : c;
	Member& other = leader.rawPointer() == &b.player() ? c : b;
	EXPECT_EQ(newLeader.count(SM_SYSTEM_MESSAGE::STR_PARTY_YOU_BECOME_NEW_LEADER()), 1);
	EXPECT_EQ(other.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_NEW_LEADER(newLeader.player().getName())), 1);
	EXPECT_EQ(other.count(SM_GROUP_INFO(group)), 1);
}

/** A group of two: the leave disbands it (PlayerGroupLeavedEvent.java:49-50, PlayerGroupService.disband, GroupDisbandEvent) */
TEST_F(PlayerGroupLifecycleTest, ALeaveFromAGroupOfTwoDisbandsIt) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	runtime::Ref<PlayerGroup> group(form({&a, &b}));
	PlayerGroupService::removePlayer(b.player());
	EXPECT_FALSE(a.player().getPlayerGroup());
	EXPECT_FALSE(b.player().getPlayerGroup());
	EXPECT_TRUE(group->isDisbanded());
	EXPECT_FALSE(PlayerGroupService::searchGroup(a.player().getObjectId())) << "groups.remove";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_LEAVE_PARTY("Bravo")), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_IS_DISPERSED()), 1) << "the nested DISBAND leave: once to the leaver himself";
	EXPECT_EQ(a.count(SM_LEAVE_GROUP_MEMBER()), 1);
	EXPECT_EQ(b.count(SM_LEAVE_GROUP_MEMBER()), 1);
	// the last-leave breaker (cycles.toml GeneralTeam.leader, D7): the group holds no Player any more
	EXPECT_FALSE(group->getLeader());
	EXPECT_EQ(group->getName(), "Leader: null");
}

/** The disband of a full group: a nested leave per member inside forEach (GroupDisbandEvent.java:22-24), with 3 and 6 members */
TEST_F(PlayerGroupLifecycleTest, DisbandRemovesEveryMemberInsideForEach) {
	for (int32_t size : {3, 6}) {
		SCOPED_TRACE(size);
		std::vector<Member*> list;
		for (int32_t i = 0; i < size; ++i)
			list.push_back(&addMember("M" + std::to_string(members.size()), 100.0f + static_cast<float>(i)));
		PlayerGroupService::createGroup(list[0]->player(), list[1]->player(), model::team::TeamType::GROUP, 0);
		runtime::Ref<PlayerGroup> group(*list[0]->player().getPlayerGroup());
		for (size_t i = 2; i < list.size(); ++i)
			PlayerGroupService::addPlayer(*group, list[i]->player());
		EXPECT_EQ(group->size(), size);
		EXPECT_TRUE(group->isFull() == (size == 6));
		clearAll();
		PlayerGroupService::disband(*group);
		EXPECT_TRUE(group->isDisbanded());
		for (Member* m : list) {
			EXPECT_FALSE(m->player().getPlayerGroup());
			EXPECT_EQ(m->count(SM_LEAVE_GROUP_MEMBER()), 1);
			// STR_PARTY_IS_DISPERSED: once to himself, once per member that left after him (PlayerGroupLeavedEvent.java:35-45, :63-65)
			EXPECT_GE(m->count(SM_SYSTEM_MESSAGE::STR_PARTY_IS_DISPERSED()), 1);
		}
	}
}

/** The leader's kick (PlayerGroupService.banPlayer :136-153) and its refusals */
TEST_F(PlayerGroupLifecycleTest, OnlyTheLeaderBansAndNotHimself) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	PlayerGroupService::banPlayer(c.player(), b.player());
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_FORCE_ONLY_LEADER_CAN_BANISH()), 1);
	PlayerGroupService::banPlayer(a.player(), a.player());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_CANT_BAN_SELF()), 1);
	EXPECT_EQ(group.size(), 3);
	clearAll();
	PlayerGroupService::banPlayer(c.player(), a.player());
	EXPECT_EQ(group.size(), 2);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_PARTY_YOU_ARE_BANISHED()), 1);
	EXPECT_EQ(c.count(SM_LEAVE_GROUP_MEMBER()), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_BANISHED("Charlie")), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_BANISHED("Charlie")), 1);
}

/** A leader change by command (ChangeGroupLeaderEvent.java:24-45) */
TEST_F(PlayerGroupLifecycleTest, ChangeLeaderTellsEveryMember) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	PlayerGroupService::changeLeader(b.player());
	EXPECT_TRUE(group.isLeader(b.player()));
	for (Member* m : {&a, &b, &c})
		EXPECT_EQ(m->count(SM_GROUP_INFO(group)), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_YOU_BECOME_NEW_LEADER()), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_NEW_LEADER("Bravo")), 1);
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_NEW_LEADER("Bravo")), 1);
}

/** Disconnect: the member stays, offline; a disconnecting leader passes leadership (PlayerDisconnectedEvent.java:29-52) */
TEST_F(PlayerGroupLifecycleTest, ADisconnectingLeaderPassesLeadershipAndStaysAnOfflineMember) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	a.player().setClientConnection(nullptr); // PlayerLeaveWorldService sets the connection null first (PlayerLeaveWorldService.java:65)
	PlayerGroupService::onPlayerLogout(a.player());
	EXPECT_EQ(group.size(), 3);
	EXPECT_EQ(a.player().getPlayerGroup().rawPointer(), &group);
	EXPECT_FALSE(group.isLeader(a.player()));
	EXPECT_TRUE(group.getLeaderObject()->isOnline());
	for (Member* m : {&b, &c}) {
		EXPECT_EQ(m->count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_BECOME_OFFLINE("Alpha")), 1);
		EXPECT_EQ(m->count(SM_GROUP_MEMBER_INFO(group, a.player(), GroupEvent::DISCONNECTED)), 1);
	}
	EXPECT_GT(group.getMember(a.player().getObjectId())->getLastOnlineTime(), 0) << "PlayerGroupService.onPlayerLogout: updateLastOnlineTime";
}

/** The last online member's logout disbands the group (PlayerDisconnectedEvent.java:33-34) */
TEST_F(PlayerGroupLifecycleTest, TheLastOnlineMembersLogoutDisbands) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	runtime::Ref<PlayerGroup> group(form({&a, &b}));
	b.player().setClientConnection(nullptr);
	PlayerGroupService::onPlayerLogout(b.player());
	EXPECT_EQ(group->size(), 2) << "one member is still online";
	a.player().setClientConnection(nullptr);
	PlayerGroupService::onPlayerLogout(a.player());
	EXPECT_TRUE(group->isDisbanded());
	EXPECT_FALSE(a.player().getPlayerGroup());
	EXPECT_FALSE(b.player().getPlayerGroup());
	a.client->enterWorld(a.f); // TearDown resets the connections of online players
	b.client->enterWorld(b.f);
}

/**
 * Relog: onPlayerLogin walks every group (E-12) and PlayerConnectedEvent replaces the member by one of the new Player object (PlayerConnectedEvent.java
 * :30-50; PlayerService.java:106 loads a new Player). A solo player's login while the group exists reaches no unported body.
 */
TEST_F(PlayerGroupLifecycleTest, ReconnectReplacesTheMemberAndASoloLoginPassesTheRegistry) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	b.player().setClientConnection(nullptr);
	PlayerGroupService::onPlayerLogout(b.player());

	Member& solo = addMember("Solo");
	runtime::resetUnportedHitsForTests();
	PlayerGroupService::onPlayerLogin(solo.player());
	EXPECT_FALSE(solo.player().getPlayerGroup());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	// the same object id logs in again with a new Player object
	Member& b2 = members.emplace_back();
	b2.f = makePlayer(b.player().getObjectId(), 9990, "Bravo");
	b2.f.player->setPosition(world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
	b2.client = std::make_unique<TestClient>();
	b2.client->enterWorld(b2.f);
	clearAll();
	PlayerGroupService::onPlayerLogin(b2.player());
	EXPECT_EQ(b2.player().getPlayerGroup().rawPointer(), &group);
	EXPECT_EQ(group.size(), 3);
	runtime::Ptr<model::team::group::PlayerGroupMember> member = group.getMember(b2.player().getObjectId());
	ASSERT_TRUE(member);
	EXPECT_EQ(&member->getPlayer(), &b2.player()) << "the member holds the new Player";
	EXPECT_FALSE(b.player().getPlayerGroup()) << "removeMember of the old member cleared the old Player's group";
	EXPECT_EQ(b2.count(SM_GROUP_INFO(group)), 1);
	EXPECT_EQ(b2.count(SM_GROUP_MEMBER_INFO(group, b2.player(), GroupEvent::JOIN)), 1);
	EXPECT_EQ(b2.count(SM_GROUP_MEMBER_INFO(group, a.player(), GroupEvent::ENTER)), 1);
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(group, b2.player(), GroupEvent::ENTER)), 1);
	EXPECT_EQ(c.count(SM_GROUP_MEMBER_INFO(group, b2.player(), GroupEvent::ENTER)), 1);
}

/** OfflinePlayerChecker (PlayerGroupService.java:210-222): scheduled with the first group, LEAVE_TIMEOUT for an expired offline member */
TEST_F(PlayerGroupLifecycleTest, TheOfflineCheckerRemovesAnExpiredOfflineMember) {
	// removetime -1: lastOnline - 1000 is already expired (TimeUtil.isExpired reads the wall clock)
	ConfigScope<int32_t> removeTime(configs::main::GroupConfig::GROUP_REMOVE_TIME, -1);
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	c.player().setClientConnection(nullptr);
	PlayerGroupService::onPlayerLogout(c.player());
	clearAll();
	executor->advance(std::chrono::milliseconds(1000)); // the checker's first run (initial delay 1000 ms)
	EXPECT_EQ(group.size(), 2);
	EXPECT_FALSE(c.player().getPlayerGroup());
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_BECOME_OFFLINE_TIMEOUT("Charlie")), 1);
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO(group, c.player(), GroupEvent::LEAVE)), 1);
	c.client->enterWorld(c.f);
}

/** A grouped player's move and stat changes reach the other members through the FIFO updaters (2,000 ms / 500 ms), never the actor */
TEST_F(PlayerGroupLifecycleTest, TheTeamUpdatersSendMovementToTheOtherMembers) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	PlayerGroup& group = form({&a, &b});
	taskmanager::tasks::TeamStatUpdater::getInstance().add(a.player());
	executor->advance(std::chrono::milliseconds(600));
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO(group, a.player(), GroupEvent::MOVEMENT)), 1);
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO_OPCODE), 0) << "PlayerGroupUpdateEvent: allExcept(player)";
	clearAll();
	taskmanager::tasks::TeamMoveUpdater::getInstance().add(b.player());
	executor->advance(std::chrono::milliseconds(2100));
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(group, b.player(), GroupEvent::MOVEMENT)), 1);
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO_OPCODE), 0);
}

/** updateGroupEffects sends UPDATE_EFFECTS with the slot to every other member (PlayerGroupService.java:101-113) */
TEST_F(PlayerGroupLifecycleTest, UpdateGroupEffectsSendsTheSlotToTheOthers) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	PlayerGroupService::updateGroupEffects(b.player(), 1);
	EXPECT_EQ(a.count(SM_GROUP_MEMBER_INFO(group, b.player(), GroupEvent::UPDATE_EFFECTS, 1)), 1);
	EXPECT_EQ(c.count(SM_GROUP_MEMBER_INFO(group, b.player(), GroupEvent::UPDATE_EFFECTS, 1)), 1);
	EXPECT_EQ(b.count(SM_GROUP_MEMBER_INFO_OPCODE), 0);
}

/** Brands: updateBrand to every member, sendBrands on (re)join (TemporaryPlayerTeam.java:40-46) */
TEST_F(PlayerGroupLifecycleTest, BrandsGoToEveryMemberAndToANewMember) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b});
	group.updateBrand(1, 4242);
	EXPECT_EQ(a.count(serverpackets::SM_SHOW_BRAND(1, 4242)), 1);
	EXPECT_EQ(b.count(serverpackets::SM_SHOW_BRAND(1, 4242)), 1);
	PlayerGroupService::addPlayer(group, c.player());
	EXPECT_EQ(c.count(serverpackets::SM_SHOW_BRAND(std::unordered_map<int32_t, int32_t>{{1, 4242}})), 1);
}

/**
 * PlayerGroupStats as Java has it (PlayerGroupStats.java:26-50): onRemovePlayer recomputes from the stale references and never clears them, so a
 * departed member stays referenced until the next onAddPlayer (D7 keeps that in a live group); the min/max exp levels are those of the last join.
 */
TEST_F(PlayerGroupLifecycleTest, GroupStatsKeepJavasStaleReferencesUntilTheNextJoin) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	b.player().getCommonData()->setLevel(5);
	PlayerGroup& group = form({&a, &b, &c});
	EXPECT_EQ(group.getMinExpPlayerLevel(), 1);
	EXPECT_EQ(group.getMaxExpPlayerLevel(), 5);
	PlayerGroupService::removePlayer(b.player());
	EXPECT_EQ(group.getMaxExpPlayerLevel(), 5) << "Java recomputes no level on a leave";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
