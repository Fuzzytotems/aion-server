// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned commands of M5d): //quest (Quest.java) and the console's ///addquest (Addquest.java) on
// a real Player with a real AionConnection (CommandTestSupport.h) in a Poeta map instance, with quest_data.xml's rows 1000 and 1107. The
// texts are the Java literals. The quest engine's own arms (QuestService.startQuest, QuestEngine.onQuestCompleted) are M5d's (tests/quests);
// the cases cover the commands' parsing, their arms over the quest state list and their packets.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/handlers/admincommands/Quest.h"
#include "aion/gameserver/handlers/consolecommands/Addquest.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/utils/ChatUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using serverpackets::SM_QUEST_ACTION;

/** quest_data/quest_data.xml, verbatim rows (DeleteQuestPacketTest.cpp's) */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<!-- :6-8 -->
	<quest id="1000" name="Prologue" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<!-- :937-942 -->
	<quest id="1107" name="The Lost Axe" nameId="1102207" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1" can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards gold="1560" exp="462"/>
		<quest_work_items>
			<quest_work_item item_id="182200501"/>
		</quest_work_items>
	</quest>
</quests>)xml";

/** ChatCommand.name (protected) */
struct QuestNamePeer : handlers::admincommands::Quest {
	using Quest::name;
};

class QuestCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["quest"] = 6; // commands.properties
		levels["addquest"] = 9;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		previousAdvParams = configs::administration::AdminConfig::CMD_QUEST_ADV_PARAMS.load();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		configs::administration::AdminConfig::CMD_QUEST_ADV_PARAMS.store(previousAdvParams);
		CommandTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	Player& gm(int32_t objectId, std::string_view name = "Warden") {
		Player& player = connected(objectId, name, 9);
		spawnInPoeta(player);
		player.setQuestStateList(model::gameobjects::player::QuestStateList::create()); // PlayerService.loadPlayer: no stored quests
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	/** a quest state in the player's list, as PlayerQuestListDAO loads it */
	runtime::Ref<QuestState> hold(Player& player, int32_t questId, QuestStatus status, int32_t questVars, int32_t flags = 0) {
		runtime::Ref<QuestState> qs = QuestState::create(questId, status, questVars, flags, 0, std::nullopt, std::nullopt, std::nullopt);
		player.getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}

	std::vector<uint8_t> questAction(SM_QUEST_ACTION::ActionType type, QuestState& qs, size_t index = 0) {
		return serialized(SM_QUEST_ACTION(type, qs), client(index).con());
	}

	static std::string quest(int32_t questId) { return utils::ChatUtil::quest(questId); }

	xml::LoadContext context;
	std::vector<Player*> stored;
	int8_t previousAdvParams = 0;
};

/** Quest.java:47-111: the syntax, an unknown player, a player without a quest, an unknown quest, a quest without an action */
TEST_F(QuestCommandsTest, Parsing) {
	Player& admin = gm(730401);
	handlers::admincommands::Quest command;
	EXPECT_TRUE(command.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"nobody", "1000", "status"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(serverpackets::SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"), client().con())}));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"warden"})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << "a player and no quest";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"warden", "4711", "status"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid quest.")) << "no such quest";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1000"})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << "a quest and no action";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1000", "jump"})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << "an unknown action";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1000", "set", "start"})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << "set without its var";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"[quest:1107]", "set", "start", "1", "6"})));
	EXPECT_EQ(client()->sentBytes(), info("[varNum] must be between 0 and 5.")) << "a quest link";
	EXPECT_EQ(admin.getQuestStateList()->getQuestState(1107), nullptr);
}

/** Quest.java:114-129, :211-230: reset and status */
TEST_F(QuestCommandsTest, ResetAndStatus) {
	Player& admin = gm(730402);
	Player& other = gm(730403, "Other");
	handlers::admincommands::Quest command;
	EXPECT_TRUE(command.process(admin, args({"other", "1107", "reset"})));
	EXPECT_EQ(client()->sentBytes(), info("Only currently active quests can be reset.")) << "no state";
	client()->clearSent();
	runtime::Ref<QuestState> qs = hold(other, 1107, QuestStatus::START, 0);
	EXPECT_TRUE(command.process(admin, args({"other", "1107", "RESET"})));
	EXPECT_EQ(client()->sentBytes(), info(QuestNamePeer::name(other) + "'s quest is already at the beginning."));
	client()->clearSent();
	qs->setQuestVarById(0, 3);
	qs->setRewardGroup(1);
	EXPECT_TRUE(command.process(admin, args({"other", "1107", "status"})));
	EXPECT_EQ(client()->sentBytes(), info("Player: " + QuestNamePeer::name(other) + ", quest: " + quest(1107) +
										   "\n\tQuest status: START\n\tQuest vars: 3 0 0 0 0 0, encoded [3]\n\tQuest flags: 0 0, encoded [0]"));
	client()->clearSent();
	client(1)->clearSent();
	EXPECT_TRUE(command.process(admin, args({"other", "1107", "reset"})));
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 0);
	EXPECT_FALSE(qs->getRewardGroup().has_value());
	EXPECT_EQ(client(1)->sentBytes(), exactly({questAction(SM_QUEST_ACTION::ActionType::UPDATE, *qs, 1)}));
	EXPECT_EQ(client()->sentBytes(), info("Reset " + quest(1107) + " for " + QuestNamePeer::name(other) + "."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1000", "status"})));
	EXPECT_EQ(client()->sentBytes(), info("Player: " + QuestNamePeer::name(admin) + ", quest: " + quest(1000) + "\n\tQuest status: NULL"))
		<< "no player: the admin (no target)";
	client()->clearSent();
	configs::administration::AdminConfig::CMD_QUEST_ADV_PARAMS.store(10);
	for (std::vector<std::string> params : {args({"1000", "status"}), args({"1000", "delete"}), args({"1000", "set", "start", "1"}),
			 args({"1000", "setflags", "1"}), args({"1000", "dialog", "1"})}) {
		client()->clearSent();
		EXPECT_TRUE(command.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info("<You need access level 10 or higher to use this function>")) << params[1];
	}
}

/** Quest.java:232-288: set, setflags, dialog */
TEST_F(QuestCommandsTest, SetFlagsAndDialog) {
	Player& admin = gm(730404);
	handlers::admincommands::Quest command;
	EXPECT_TRUE(command.process(admin, args({"1107", "set", "start", "2"})));
	runtime::Ptr<QuestState> qs = admin.getQuestStateList()->getQuestState(1107);
	ASSERT_NE(qs, nullptr) << "a new state";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 2) << "QuestState.setQuestVar: the encoded value (var 0 = 2)";
	std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), questAction(SM_QUEST_ACTION::ActionType::ADD, *qs));
	EXPECT_EQ(std::vector<std::vector<uint8_t>>(sent.end() - 1, sent.end()), info("Set quest status of " + quest(1107) + " for " + QuestNamePeer::name(admin) + "."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1107", "set", "start", "5", "1"})));
	EXPECT_EQ(qs->getQuestVarById(1), 5);
	EXPECT_EQ(qs->getQuestVarById(0), 2) << "only varNum 1";
	sent = client()->sentBytes();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), questAction(SM_QUEST_ACTION::ActionType::UPDATE, *qs)) << "a held quest: UPDATE";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1107", "setflags", "9"})));
	EXPECT_EQ(qs->getFlags(), 9);
	EXPECT_EQ(client()->sentBytes(), exactly({questAction(SM_QUEST_ACTION::ActionType::UPDATE, *qs), message("Set " + QuestNamePeer::name(admin) + "'s quest flags to 9.")}));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1000", "setflags", "9"})));
	EXPECT_EQ(client()->sentBytes(), info("Flags can only be set for active quests."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"1107", "dialog", "1011"})));
	EXPECT_EQ(client()->sentBytes(),
		exactly({serialized(serverpackets::SM_DIALOG_WINDOW(0, 1011, 1107), client().con()), message("Sent dialog page 1011 of Q1107.")}));
}

/** Quest.java:192-209: delete */
TEST_F(QuestCommandsTest, Delete) {
	Player& admin = gm(730405);
	Player& other = gm(730406, "Other");
	handlers::admincommands::Quest command;
	EXPECT_TRUE(command.process(admin, args({"other", "1107", "delete"})));
	EXPECT_EQ(client()->sentBytes(), info(QuestNamePeer::name(other) + " does not have that quest."));
	client()->clearSent();
	runtime::Ref<QuestState> qs = hold(other, 1107, QuestStatus::START, 1);
	client(1)->clearSent();
	EXPECT_TRUE(command.process(admin, args({"other", "1107", "delete"})));
	EXPECT_EQ(other.getQuestStateList()->getQuestState(1107), nullptr);
	std::vector<std::vector<uint8_t>> sent = client(1)->sentBytes();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), questAction(SM_QUEST_ACTION::ActionType::ABANDON, *qs, 1));
	EXPECT_EQ(client()->sentBytes(), info("Deleted " + quest(1107) + " for " + QuestNamePeer::name(other) + "."));
	client()->clearSent();
	hold(admin, 1107, QuestStatus::START, 1);
	EXPECT_TRUE(command.process(admin, args({"1107", "delete"})));
	EXPECT_EQ(admin.getQuestStateList()->getQuestState(1107), nullptr);
	const std::vector<std::vector<uint8_t>> deleted = info("Deleted " + quest(1107) + " for " + QuestNamePeer::name(admin) + ".");
	for (const std::vector<uint8_t>& packet : client()->sentBytes())
		EXPECT_NE(packet, deleted.front()) << "on himself: no answer";
}

/** Addquest.java:28-78: the syntax, a word, a quest link */
TEST_F(QuestCommandsTest, AddquestParsing) {
	Player& admin = gm(730407);
	handlers::consolecommands::Addquest command;
	const std::vector<uint8_t> syntax = message("syntax ///addquest <id quest>");
	EXPECT_TRUE(command.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "NumberFormatException";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"[quest:x1]"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "a link without a number";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
