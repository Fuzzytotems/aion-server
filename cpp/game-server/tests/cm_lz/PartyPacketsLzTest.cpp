// The party client packets of P5-16 (m5g-plan.md K-01, K-02, K-04, K-06): CM_PLAYER_STATUS_INFO, CM_SHOW_BRAND and CM_QUEST_SHARE, read from
// their client bytes and run against real Players on recording connections (the party fixture of tests/team/P5-10b by relative path).
//
// Expectations are derived by hand from CM_PLAYER_STATUS_INFO.java:31-55, CM_SHOW_BRAND.java:31-46, CM_QUEST_SHARE.java:39-82 and the services
// they call.

#include "../team/P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PLAYER_STATUS_INFO.h"
#include "aion/gameserver/network/aion/clientpackets/CM_QUEST_SHARE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_BRAND.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEAVE_GROUP_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using serverpackets::SM_SHOW_BRAND;

// the decoded opcodes of ClientPacketInfo.gen.inc
constexpr int32_t CM_PLAYER_STATUS_INFO_OPCODE = 96; // :96
constexpr int32_t CM_SHOW_BRAND_OPCODE = 181;        // :159
constexpr int32_t CM_QUEST_SHARE_OPCODE = 164;       // :147

// two Poeta quests of quest_data.xml, reduced to the attributes CM_QUEST_SHARE and checkStartConditions read
constexpr std::string_view QUESTS_XML = R"xml(<quests>
	<quest id="1102" name="Kerubar Hunt" nameId="1102202" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<quest id="1105" name="The Snuffler Headache" nameId="1102205" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<!-- hand-made: 1105 with minlevel_permitted 10, so a level-1 member fails checkStartConditions at its level check, before the item and
	     npc faction checks reach the database this unit test has none of -->
	<quest id="9105" name="Shareable Above Ten" nameId="1102205" quest_zone="Poeta" minlevel_permitted="10" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
</quests>)xml";

class PartyPacketsLzTest : public TeamTest {
protected:
	template <class P>
	void run(int32_t opcode, Member& sender, const PacketWriter& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body.data, sender.client->get());
	}

	PacketWriter statusInfo(int32_t command, int32_t selected) { return PacketWriter().C(command).D(selected).D(0).D(0); }
};

/** CM_PLAYER_STATUS_INFO.java:39-55: 6 leave (member 0 = self), 3 set leader, 2 ban; 9 sets the LFG flag */
TEST_F(PartyPacketsLzTest, PlayerStatusInfoRunsTheGroupCommands) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	Member& c = addMember("Charlie");
	PlayerGroup& group = form({&a, &b, &c});
	run<CM_PLAYER_STATUS_INFO>(CM_PLAYER_STATUS_INFO_OPCODE, a, statusInfo(3, b.player().getObjectId()));
	EXPECT_TRUE(group.isLeader(b.player()));
	run<CM_PLAYER_STATUS_INFO>(CM_PLAYER_STATUS_INFO_OPCODE, b, statusInfo(2, c.player().getObjectId()));
	EXPECT_FALSE(c.player().getPlayerGroup());
	EXPECT_EQ(c.count(SM_SYSTEM_MESSAGE::STR_PARTY_YOU_ARE_BANISHED()), 1);
	run<CM_PLAYER_STATUS_INFO>(CM_PLAYER_STATUS_INFO_OPCODE, a, statusInfo(6, 0));
	EXPECT_FALSE(a.player().getPlayerGroup());
	EXPECT_FALSE(b.player().getPlayerGroup()) << "a group of one is disbanded";
	run<CM_PLAYER_STATUS_INFO>(CM_PLAYER_STATUS_INFO_OPCODE, a, statusInfo(9, 2));
	EXPECT_TRUE(a.player().isLookingForGroup());
	run<CM_PLAYER_STATUS_INFO>(CM_PLAYER_STATUS_INFO_OPCODE, a, statusInfo(9, 1));
	EXPECT_FALSE(a.player().isLookingForGroup());
	// an unknown command code is TeamCommand.getCommand's NullPointerException
	EXPECT_THROW(run<CM_PLAYER_STATUS_INFO>(CM_PLAYER_STATUS_INFO_OPCODE, a, statusInfo(7, 0)), runtime::NullPointerException);
}

/** CM_SHOW_BRAND.java:38-46: solo to himself; in a team only the leader marks, for every member */
TEST_F(PartyPacketsLzTest, ShowBrandMarksForTheTeamOnlyFromTheLeader) {
	Member& a = addMember("Alpha");
	Member& b = addMember("Bravo");
	run<CM_SHOW_BRAND>(CM_SHOW_BRAND_OPCODE, a, PacketWriter().D(0).D(1).D(4242));
	EXPECT_EQ(a.count(SM_SHOW_BRAND(1, 4242)), 1);
	EXPECT_EQ(b.count(SM_SHOW_BRAND_OPCODE), 0);
	form({&a, &b});
	run<CM_SHOW_BRAND>(CM_SHOW_BRAND_OPCODE, b, PacketWriter().D(0).D(2).D(4242));
	EXPECT_EQ(a.count(SM_SHOW_BRAND_OPCODE) + b.count(SM_SHOW_BRAND_OPCODE), 0) << "a member who is not the leader marks nothing";
	run<CM_SHOW_BRAND>(CM_SHOW_BRAND_OPCODE, a, PacketWriter().D(0).D(1).D(4242));
	EXPECT_EQ(a.count(SM_SHOW_BRAND(1, 4242)), 1);
	EXPECT_EQ(b.count(SM_SHOW_BRAND(1, 4242)), 1);
}

class QuestSharePacketTest : public PartyPacketsLzTest {
protected:
	void SetUp() override {
		PartyPacketsLzTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUESTS_XML));
	}

	void TearDown() override {
		PartyPacketsLzTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	Member& addQuester(std::string_view name) {
		Member& m = addMember(name);
		m.player().setQuestStateList(model::gameobjects::player::QuestStateList::create());
		return m;
	}

	void hold(Member& m, int32_t questId) {
		runtime::Ref<QuestState> qs = QuestState::create(questId, QuestStatus::START, 0, 0, 0, std::nullopt, std::nullopt, std::nullopt);
		m.player().getQuestStateList()->addQuest(questId, *qs);
	}
};

/**
 * CM_QUEST_SHARE.java:44-82: a cannot_share quest is refused with 1100001; a shareable quest held by a solo player has nobody to share with
 * (1100000); in a group every other online member within range is checked - here each fails QuestService.checkStartConditions at the minimum
 * level, so the sharer is told 1100003 once per member, never about himself, and nobody gets an offer. The offer itself (SM_QUEST_ACTION and
 * 1100002) passes the item and npc faction checks, which read the database: the gate's GP15b asserts it.
 */
TEST_F(QuestSharePacketTest, QuestShareChecksEveryOtherMemberInRange) {
	ConfigScope<int32_t> maxDistance(configs::main::GroupConfig::GROUP_MAX_DISTANCE, 100); // gameserver.playergroup.maxdistance (no properties here)
	Member& b = addQuester("Bravo");
	Member& a = addQuester("Alpha");
	Member& c = addQuester("Charlie");
	hold(b, 1102);
	hold(b, 9105);
	run<CM_QUEST_SHARE>(CM_QUEST_SHARE_OPCODE, b, PacketWriter().D(1102));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE(1100001, std::vector<std::string>())), 1);
	run<CM_QUEST_SHARE>(CM_QUEST_SHARE_OPCODE, b, PacketWriter().D(9105));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE(1100000, std::vector<std::string>())), 1);
	form({&b, &a, &c});
	run<CM_QUEST_SHARE>(CM_QUEST_SHARE_OPCODE, b, PacketWriter().D(9105));
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE(1100003, std::string("Alpha"))), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE(1100003, std::string("Charlie"))), 1);
	EXPECT_EQ(b.count(SM_SYSTEM_MESSAGE(1100003, std::string("Bravo"))), 0) << "allExcept(player)";
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_QUEST_ACTION>) + c.count(opcodeOf<serverpackets::SM_QUEST_ACTION>), 0);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
