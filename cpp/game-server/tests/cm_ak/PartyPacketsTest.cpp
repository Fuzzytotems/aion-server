// The party client packets of P5-15 (m5g-plan.md K-01..K-03, K-06): CM_INVITE_TO_GROUP, CM_DISTRIBUTION_SETTINGS, CM_GROUP_DISTRIBUTION,
// CM_GROUP_LOOT, CM_GROUP_DATA_EXCHANGE and CM_FIND_GROUP, read from their client bytes and run against real Players on recording connections
// (the party fixture of tests/team/P5-10b, included by relative path as tests/cm_lz includes tests/cm_ak's support).
//
// Expectations are derived by hand from the Java packets (CM_INVITE_TO_GROUP.java:30-68, CM_DISTRIBUTION_SETTINGS.java:41-78,
// CM_GROUP_DISTRIBUTION.java:29-56, CM_GROUP_LOOT.java:44-64, CM_GROUP_DATA_EXCHANGE.java:36-88, CM_FIND_GROUP.java:39-142) and the services
// they call. CM_CHAT_MESSAGE_PUBLIC is lane A's (m5g-plan.md K-04 there).

#include "../team/P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DISTRIBUTION_SETTINGS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_FIND_GROUP.h"
#include "aion/gameserver/network/aion/clientpackets/CM_GROUP_DATA_EXCHANGE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_GROUP_DISTRIBUTION.h"
#include "aion/gameserver/network/aion/clientpackets/CM_GROUP_LOOT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_INVITE_TO_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FIND_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_DATA_EXCHANGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_GROUP_DATA_EXCHANGE;
using serverpackets::SM_GROUP_INFO;
using serverpackets::SM_QUESTION_WINDOW;

// the decoded opcodes of ClientPacketInfo.gen.inc
constexpr int32_t CM_INVITE_TO_GROUP_OPCODE = 97;        // :97
constexpr int32_t CM_DISTRIBUTION_SETTINGS_OPCODE = 185; // :162
constexpr int32_t CM_GROUP_DISTRIBUTION_OPCODE = 108;    // :102
constexpr int32_t CM_GROUP_LOOT_OPCODE = 184;            // :161
constexpr int32_t CM_GROUP_DATA_EXCHANGE_OPCODE = 79;    // :86
constexpr int32_t CM_FIND_GROUP_OPCODE = 77;             // :84
constexpr int32_t SM_FIND_GROUP_OPCODE = opcodeOf<serverpackets::SM_FIND_GROUP>;
constexpr int32_t SM_GROUP_DATA_EXCHANGE_OPCODE = opcodeOf<serverpackets::SM_GROUP_DATA_EXCHANGE>;

class PartyPacketsTest : public TeamTest {
protected:
	void TearDown() override {
		for (Member& m : members)
			world::World::getInstance().removeObject(*m.f.player);
		TeamTest::TearDown();
	}

	/** addMember plus World.storeObject, so World.getPlayer(name) finds him */
	Member& addStoredMember(std::string_view name, float x = 100.0f) {
		Member& m = addMember(name, x);
		world::World::getInstance().storeObject(*m.f.player);
		return m;
	}

	template <class P>
	void run(int32_t opcode, Member& sender, const PacketWriter& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body.data, sender.client->get());
	}
};

/** CM_INVITE_TO_GROUP.java:36-49: a dead inviter and an unknown name are refused before any service runs */
TEST_F(PartyPacketsTest, InviteRefusesADeadInviterAndAnUnknownName) {
	Member& a = addStoredMember("Alpha");
	run<CM_INVITE_TO_GROUP>(CM_INVITE_TO_GROUP_OPCODE, a, PacketWriter().C(0).S("Nosuchname"));
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nosuchname")), 1);
}

/** inviteType 0 asks the invited player (PlayerGroupService.inviteToGroup); a denied GROUP status refuses (CM_INVITE_TO_GROUP.java:51-54) */
TEST_F(PartyPacketsTest, InviteTypeZeroAsksTheInvitedPlayerUnlessHeDeniesGroups) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	run<CM_INVITE_TO_GROUP>(CM_INVITE_TO_GROUP_OPCODE, a, PacketWriter().C(0).S("Bravo"));
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_PARTY_DO_YOU_ACCEPT_INVITATION, 0, 0, std::string("Alpha"))), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_PARTY_INVITED_HIM("Bravo")), 1);
	clearAll();
	b.player().getResponseRequester().denyAll();
	b.player().getPlayerSettings()->setDeny(model::gameobjects::player::getId(model::gameobjects::player::DeniedStatus::GROUP));
	run<CM_INVITE_TO_GROUP>(CM_INVITE_TO_GROUP_OPCODE, a, PacketWriter().C(0).S("Bravo"));
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW_OPCODE), 0);
}

/** CM_INVITE_TO_GROUP.java:61-63: invite type 12 reaches PlayerAllianceService.inviteToAlliance - the alliance question to a solo player
 * (PlayerAllianceService.java:42-62; ported by the alliance lane, m5g-plan.md §16) */
TEST_F(PartyPacketsTest, InviteTypeTwelveReachesTheAllianceService) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	run<CM_INVITE_TO_GROUP>(CM_INVITE_TO_GROUP_OPCODE, a, PacketWriter().C(12).S("Bravo"));
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_PARTY_ALLIANCE_DO_YOU_ACCEPT_HIS_INVITATION, 0, 0, std::string("Alpha"))), 1);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_FORCE_INVITED_HIM("Bravo")), 1);
}

/** CM_DISTRIBUTION_SETTINGS.java:63-71: any member sets new rules, every member receives SM_GROUP_INFO */
TEST_F(PartyPacketsTest, DistributionSettingsChangeTheGroupsLootRules) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	PlayerGroup& group = form({&a, &b});
	run<CM_DISTRIBUTION_SETTINGS>(CM_DISTRIBUTION_SETTINGS_OPCODE, b,
		PacketWriter().D(0).D(0).D(0).D(0).D(0).D(0).D(0).D(0).D(0).D(0));
	EXPECT_EQ(group.getLootGroupRules()->getLootRule(), model::team::common::legacy::LootRuleType::FREEFORALL);
	EXPECT_EQ(group.getLootGroupRules()->getSuperiorItemAbove(), 0);
	EXPECT_EQ(group.getLootGroupRules()->getNrRoundRobin(), 0) << "a new LootGroupRules restarts the round robin";
	EXPECT_EQ(a.count(SM_GROUP_INFO(group)), 1);
	EXPECT_EQ(b.count(SM_GROUP_INFO(group)), 1);
	// an unknown rule id reads as FREEFORALL (CM_DISTRIBUTION_SETTINGS.java:44-49)
	run<CM_DISTRIBUTION_SETTINGS>(CM_DISTRIBUTION_SETTINGS_OPCODE, a,
		PacketWriter().D(0).D(1).D(0).D(0).D(2).D(2).D(2).D(2).D(2).D(0));
	EXPECT_EQ(group.getLootGroupRules()->getLootRule(), model::team::common::legacy::LootRuleType::ROUNDROBIN);
	run<CM_DISTRIBUTION_SETTINGS>(CM_DISTRIBUTION_SETTINGS_OPCODE, a,
		PacketWriter().D(0).D(7).D(0).D(0).D(2).D(2).D(2).D(2).D(2).D(0));
	EXPECT_EQ(group.getLootGroupRules()->getLootRule(), model::team::common::legacy::LootRuleType::FREEFORALL);
}

/** CM_GROUP_DISTRIBUTION.java:35-56: amounts below 2 are ignored; partyType 1 of a group member splits in the group */
TEST_F(PartyPacketsTest, GroupDistributionSplitsKinahInTheGroup) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	form({&a, &b});
	giveKinah(a, 731001, 100);
	giveKinah(b, 731002, 0);
	run<CM_GROUP_DISTRIBUTION>(CM_GROUP_DISTRIBUTION_OPCODE, a, PacketWriter().Q(1).C(1));
	EXPECT_EQ(a.player().getInventory().getKinah(), 100);
	run<CM_GROUP_DISTRIBUTION>(CM_GROUP_DISTRIBUTION_OPCODE, a, PacketWriter().Q(10).C(1));
	EXPECT_EQ(a.player().getInventory().getKinah(), 95);
	EXPECT_EQ(b.player().getInventory().getKinah(), 5);
}

/** CM_GROUP_LOOT.java:44-64: the fields are read and handed to DropDistributionService, which ignores an npc without registered drops */
TEST_F(PartyPacketsTest, GroupLootReadsItsFieldsAndIgnoresAnUnknownCorpse) {
	Member& a = addStoredMember("Alpha");
	runtime::resetUnportedHitsForTests();
	run<CM_GROUP_LOOT>(CM_GROUP_LOOT_OPCODE, a,
		PacketWriter().D(1).D(2).D(0).D(182400001).C(0).C(0).C(0).D(4711).C(2).D(1).Q(0));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	EXPECT_TRUE(a.sent().empty());
}

/**
 * CM_GROUP_DATA_EXCHANGE.java:61-87: action 1 goes to the known list and the sender; any other action to the other online members of the group
 * only - never to the sender, never to an outsider who knows him.
 */
TEST_F(PartyPacketsTest, GroupDataExchangeReachesTheOtherMembersOrTheKnownList) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	Member& c = addStoredMember("Charlie");
	Member& d = addStoredMember("Delta");
	a.f.knownList().addForTest(d.player());
	d.f.knownList().addForTest(a.player());
	form({&a, &b, &c});
	const std::vector<uint8_t> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	run<CM_GROUP_DATA_EXCHANGE>(CM_GROUP_DATA_EXCHANGE_OPCODE, a, PacketWriter().C(0).C(0).C(0).D(16).B(data));
	EXPECT_EQ(b.count(SM_GROUP_DATA_EXCHANGE(data, 0, 0)), 1);
	EXPECT_EQ(c.count(SM_GROUP_DATA_EXCHANGE(data, 0, 0)), 1);
	EXPECT_EQ(a.count(SM_GROUP_DATA_EXCHANGE_OPCODE), 0) << "never to the sender";
	EXPECT_EQ(d.count(SM_GROUP_DATA_EXCHANGE_OPCODE), 0) << "never to an outsider";
	clearAll();
	run<CM_GROUP_DATA_EXCHANGE>(CM_GROUP_DATA_EXCHANGE_OPCODE, a, PacketWriter().C(1).D(16).B(data));
	EXPECT_EQ(a.count(SM_GROUP_DATA_EXCHANGE(data)), 1) << "broadcastPacketAndReceive: the sender too";
	EXPECT_EQ(d.count(SM_GROUP_DATA_EXCHANGE(data)), 1) << "the known list";
	EXPECT_EQ(b.count(SM_GROUP_DATA_EXCHANGE_OPCODE), 0) << "B does not know A here";
	// an empty payload is ignored (CM_GROUP_DATA_EXCHANGE.java:49-50)
	clearAll();
	run<CM_GROUP_DATA_EXCHANGE>(CM_GROUP_DATA_EXCHANGE_OPCODE, a, PacketWriter().C(1).D(0));
	EXPECT_EQ(a.count(SM_GROUP_DATA_EXCHANGE_OPCODE), 0);
}

/**
 * CM_FIND_GROUP (CM_FIND_GROUP.java:39-142) with FindGroupService: a recruitment is posted and listed for the own race, and FindGroupService.
 * onJoinedTeam removes a player's own recruitment once he joins a team (FindGroupService.java:182-194). 20 and 25 are read and ignored; an
 * unknown action only logs.
 */
TEST_F(PartyPacketsTest, FindGroupPostsListsAndRemovesARecruitmentOnJoin) {
	Member& a = addStoredMember("Alpha");
	Member& d = addStoredMember("Delta");
	run<CM_FIND_GROUP>(CM_FIND_GROUP_OPCODE, d, PacketWriter().C(2).D(d.player().getObjectId()).S("m5g lfg").C(0));
	EXPECT_EQ(d.count(SM_SYSTEM_MESSAGE::STR_PARTY_MATCH_OFFER_PARTY_POSTED()), 1);
	EXPECT_EQ(d.count(SM_FIND_GROUP_OPCODE), 1) << "showRecruitments after posting";
	clearAll();
	run<CM_FIND_GROUP>(CM_FIND_GROUP_OPCODE, a, PacketWriter().C(0));
	ASSERT_EQ(a.count(SM_FIND_GROUP_OPCODE), 1);
	// D joins a group: his recruitment is removed (a world broadcast of the removal to his race)
	PlayerGroupService::createGroup(a.player(), d.player(), model::team::TeamType::GROUP, 0);
	clearAll();
	EXPECT_FALSE(services::findgroup::FindGroupService::getInstance().removeRecruitment(d.player(), 0, 0, 0, 0))
		<< "onJoinedTeam removed D's recruitment; nothing is left under his team id";
	runtime::resetUnportedHitsForTests();
	LogCapture log({"com.aionemu.gameserver.network.aion.clientpackets.CM_FIND_GROUP"});
	run<CM_FIND_GROUP>(CM_FIND_GROUP_OPCODE, a, PacketWriter().C(20));
	run<CM_FIND_GROUP>(CM_FIND_GROUP_OPCODE, a, PacketWriter().C(25).D(1).D(2).D(3));
	EXPECT_FALSE(log.contains("Unknown find group action"));
	run<CM_FIND_GROUP>(CM_FIND_GROUP_OPCODE, a, PacketWriter().C(14));
	EXPECT_TRUE(log.contains("Unknown find group action 14"));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
