// C1 cp2 (lane A, 2026-10-18): the player commands .lock, .pvp and .questrestart (data/handlers/playercommands), on real Players with real
// AionConnections (CommandTestSupport.h). The texts are the Java literals. Left to the gates and the retest: the login server's answer to
// SM_CHANGE_ALLOWED_HDD_SERIAL (no login server link here) and joining or leaving the PvP map.

#include "CommandTestSupport.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/handlers/playercommands/Lock.h"
#include "aion/gameserver/handlers/playercommands/Pvp.h"
#include "aion/gameserver/handlers/playercommands/Questrestart.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using serverpackets::SM_QUEST_ACTION;

/** quest rows for the ids the .questrestart case uses (quest_data.xml's shape; SM_QUEST_ACTION reads the template) */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<quest id="1006" name="Quest 1006" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<quest id="1101" name="Quest 1101" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<quest id="1102" name="Quest 1102" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<quest id="1103" name="Quest 1103" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
	<quest id="1104" name="Quest 1104" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" race_permitted="ELYOS" category="QUEST">
		<rewards exp="1"/>
	</quest>
</quests>)xml";

class PlayerToolCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		xml::LoadContext questContext;
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(questContext, QUEST_DATA_XML));
	}

	void TearDown() override {
		CommandTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	runtime::Ref<QuestState> quest(Player& player, int32_t questId, QuestStatus status, int32_t var) {
		runtime::Ref<QuestState> qs = QuestState::create(questId, status);
		qs->setQuestVarById(0, var);
		player.getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}
};

/** Lock.java:20-42: the syntax; enable needs the client's HDD serial; disable clears it; an unknown parameter */
TEST_F(PlayerToolCommandsTest, LockSetsAndClearsTheAllowedSerial) {
	Player& player = connected(733200, "Lockie", 0);
	handlers::playercommands::Lock command;
	command.execute(player, args({}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	player.getClientConnection()->setHddSerial("");
	command.execute(player, args({"ENABLE"}));
	EXPECT_EQ(client()->sentBytes(), info("Couldn't lock your account. Please re-log and try again."));
	EXPECT_FALSE(player.getAccount()->getAllowedHddSerial().has_value());
	client()->clearSent();
	player.getClientConnection()->setHddSerial("HDD-1234");
	command.execute(player, args({"enable"}));
	EXPECT_EQ(client()->sentBytes(),
		info("Your account is now locked. You will not be able to login from any other computer from now on."));
	EXPECT_EQ(player.getAccount()->getAllowedHddSerial(), std::optional<std::string>("HDD-1234"));
	client()->clearSent();
	command.execute(player, args({"disable"}));
	EXPECT_EQ(client()->sentBytes(), info("Your account is unlocked. You can login from any computer again."));
	EXPECT_FALSE(player.getAccount()->getAllowedHddSerial().has_value());
	client()->clearSent();
	command.execute(player, args({"other"}));
	EXPECT_EQ(client()->sentBytes(), info("Invalid parameter."));
}

/** Pvp.java:19-33: the syntax for nothing or an unknown parameter; info counts the players on the map */
TEST_F(PlayerToolCommandsTest, PvpTellsTheMapsPlayers) {
	Player& player = connected(733210, "Fighter", 0);
	handlers::playercommands::Pvp command;
	command.execute(player, args({}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(player, args({"other"}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(player, args({"INFO"}));
	EXPECT_EQ(client()->sentBytes(), info("There are currently no players on the map."));
}

/** Questrestart.java:21-55: the refusals in their order, then a restart back to the first step */
TEST_F(PlayerToolCommandsTest, QuestrestartRestartsAnActiveQuest) {
	Player& player = connected(733220, "Quester", 0);
	handlers::playercommands::Questrestart command;
	command.execute(player, args({}));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	command.execute(player, args({"abc"}));
	EXPECT_EQ(client()->sentBytes(), info("Invalid quest."));
	client()->clearSent();
	command.execute(player, args({"1100"}));
	EXPECT_EQ(client()->sentBytes(), info("Only currently active quests can be restarted.")) << "no state";
	quest(player, 1101, QuestStatus::COMPLETE, 3);
	client()->clearSent();
	command.execute(player, args({"1101"}));
	EXPECT_EQ(client()->sentBytes(), info("Only currently active quests can be restarted."));
	quest(player, 1006, QuestStatus::START, 3);
	client()->clearSent();
	command.execute(player, args({"1006"}));
	EXPECT_EQ(client()->sentBytes(), info("Quest [quest:1006] can't be restarted."));
	quest(player, 1102, QuestStatus::REWARD, 1);
	client()->clearSent();
	command.execute(player, args({"1102"}));
	EXPECT_EQ(client()->sentBytes(), info("Quest [quest:1102] can't be restarted.")) << "reward with vars 1";
	quest(player, 1103, QuestStatus::START, 0);
	client()->clearSent();
	command.execute(player, args({"[quest:1103]"}));
	EXPECT_EQ(client()->sentBytes(), info("Restarting quest [quest:1103] would have no effect."));
	runtime::Ref<QuestState> qs = quest(player, 1104, QuestStatus::REWARD, 5);
	qs->setFlags(7);
	qs->setRewardGroup(2);
	client()->clearSent();
	command.execute(player, args({"1104"}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	EXPECT_EQ(qs->getFlags(), 0);
	EXPECT_FALSE(qs->getRewardGroup().has_value());
	std::vector<std::vector<uint8_t>> expected{serialized(SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs), client().con())};
	for (std::vector<uint8_t>& part : info("Quest [quest:1104] was restarted."))
		expected.push_back(std::move(part));
	EXPECT_EQ(client()->sentBytes(), expected);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
