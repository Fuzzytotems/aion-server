// P5-13 PlayerRestrictions.canInviteToGroup / canInviteToTeam (m5g-plan.md W-01, W-05): the checks of PlayerRestrictions.java:126-205 in their
// order, each with the message Java sends, on real Players of the party fixture (tests/team/P5-10b, by relative path).
//
// The alliance-only arms (vice captains, the group-into-alliance slot count, defence alliances) need PlayerAlliance bodies, the alliance lane's
// (m5g-plan.md AL-01); the prison and FFA arms need state no fixture sets without the services that own it.

#include "../team/P5-10b/TeamTestSupport.h"

#include <cstdint>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using restrictions::PlayerRestrictions;

class PartyInviteRestrictionsTest : public TeamTest {};

TEST_F(PartyInviteRestrictionsTest, ASoloPlayerMayInviteAnotherSoloPlayerOfHisRace) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	EXPECT_TRUE(PlayerRestrictions::canInviteToGroup(a.player(), b.player()));
	EXPECT_TRUE(a.sent().empty());
}

/** PlayerRestrictions.java:127-130: a dead inviter */
TEST_F(PartyInviteRestrictionsTest, ADeadInviterIsRefused) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	a.player().setLifeStats(std::make_unique<DeadPlayerLifeStats>(a.player()));
	ASSERT_TRUE(a.player().isDead());
	clearAll();
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(a.player(), b.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_CANT_INVITE_WHEN_DEAD()), 1);
}

/** :149-152 before :158-161: a member who is not the leader is told so even when he invites himself (the order GP4 asserts) */
TEST_F(PartyInviteRestrictionsTest, TheLeaderCheckPrecedesTheSelfCheck) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& d = addMember("Delta");
	form({&a, &b});
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(b.player(), d.player()));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_ONLY_LEADER_CAN_INVITE()), 1);
	clearAll();
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(b.player(), b.player()));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_ONLY_LEADER_CAN_INVITE()), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE::STR_PARTY_CAN_NOT_INVITE_SELF()), 0);
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(a.player(), a.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_CAN_NOT_INVITE_SELF()), 1);
}

/** :153-156: a full group of six */
TEST_F(PartyInviteRestrictionsTest, AFullGroupIsRefused) {
	std::vector<Member*> six;
	for (int32_t i = 0; i < 6; ++i)
		six.push_back(&addMember("M" + std::to_string(i)));
	Member& outsider = addMember("Outsider");
	PlayerGroupService::createGroup(six[0]->player(), six[1]->player(), model::team::TeamType::GROUP, 0);
	for (size_t i = 2; i < six.size(); ++i)
		PlayerGroupService::addPlayer(*six[0]->player().getPlayerGroup(), six[i]->player());
	clearAll();
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(six[0]->player(), outsider.player()));
	EXPECT_EQ(six[0]->count(SM_SYSTEM_MESSAGE::STR_PARTY_CANT_ADD_NEW_MEMBER()), 1);
}

/** :162-165: the other race, unless gameserver.playergroup.invite_other_faction (GroupConfig.GROUP_INVITEOTHERFACTION) */
TEST_F(PartyInviteRestrictionsTest, TheOtherRaceIsRefusedUnlessConfigured) {
	Member& a = addMember("Alpha");
	Member& asmo = members.emplace_back();
	asmo.f = makePlayer(729001, 9981, "Asmo", model::Race::ASMODIANS);
	asmo.f.player->setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
	asmo.client = std::make_unique<TestClient>();
	asmo.client->enterWorld(asmo.f);
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(a.player(), asmo.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_CANT_INVITE_OTHER_RACE()), 1);
	ConfigScope<bool> otherFaction(configs::main::GroupConfig::GROUP_INVITEOTHERFACTION, true);
	EXPECT_TRUE(PlayerRestrictions::canInviteToGroup(a.player(), asmo.player()));
}

/** :170-185: the target's team - the inviter's own (already a member) or another group */
TEST_F(PartyInviteRestrictionsTest, ATargetInATeamIsRefused) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	Member& d = addMember("Delta");
	form({&a, &b});
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(a.player(), b.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OUR_PARTY("Bravo")), 1);
	PlayerGroupService::createGroup(c.player(), d.player(), model::team::TeamType::GROUP, 0);
	clearAll();
	EXPECT_FALSE(PlayerRestrictions::canInviteToGroup(a.player(), c.player()));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OTHER_PARTY("Charlie")), 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
