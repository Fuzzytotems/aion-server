// The stage-0 character commands (m5j-plan.md §5.2): //addexp, //set, //addskill, //delskill, //heal, //speed, //dispel, //morph, //state,
// //stat (data/handlers/admincommands) and the console ///levelup, ///leveldown (data/handlers/consolecommands), on a real Player with a real
// AionConnection (CommandTestSupport.h). The texts are the Java literals.

#include "CommandTestSupport.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/handlers/admincommands/AddExp.h"
#include "aion/gameserver/handlers/admincommands/AddSkill.h"
#include "aion/gameserver/handlers/admincommands/DelSkill.h"
#include "aion/gameserver/handlers/admincommands/Dispel.h"
#include "aion/gameserver/handlers/admincommands/Heal.h"
#include "aion/gameserver/handlers/admincommands/Morph.h"
#include "aion/gameserver/handlers/admincommands/RemoveCd.h"
#include "aion/gameserver/handlers/admincommands/Set.h"
#include "aion/gameserver/handlers/admincommands/Speed.h"
#include "aion/gameserver/handlers/admincommands/Stat.h"
#include "aion/gameserver/handlers/admincommands/State.h"
#include "aion/gameserver/handlers/consolecommands/Clearusercoolt.h"
#include "aion/gameserver/handlers/consolecommands/Leveldown.h"
#include "aion/gameserver/handlers/consolecommands/Levelup.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::stats::container::StatEnum;
using serverpackets::SM_SYSTEM_MESSAGE;

class CharacterCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		previousMaxLevel = configs::main::GSConfig::PLAYER_MAX_LEVEL.load();
		configs::main::GSConfig::PLAYER_MAX_LEVEL.store(15); // the fixture's experience table has 16 levels
		// a level change learns the skills of the new level (SkillLearnService.learnNewSkills): none here
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(skillTreeContext, "<skill_tree/>"));
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		configs::main::GSConfig::PLAYER_MAX_LEVEL.store(previousMaxLevel);
		CommandTest::TearDown();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
	}

	/** a connected character that World lists (getPlayer by name, PlayerCommonData.getPlayer) */
	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel) {
		Player& player = connected(objectId, name, accessLevel);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		player.getCommonData()->setOnline(true);
		stored.push_back(&player);
		return player;
	}

	bool sent(const std::vector<uint8_t>& packet, size_t index = 0) {
		const std::vector<std::vector<uint8_t>> all = client(index)->sentBytes();
		return std::ranges::find(all, packet) != all.end();
	}

	int32_t previousMaxLevel = 0;
	xml::LoadContext skillTreeContext;
	std::vector<Player*> stored;
};

// ---- //addexp, //set, ///levelup, ///leveldown --------------------------------------------------------------------------------------------

TEST_F(CharacterCommandsTest, AddExpAddsAndFloorsAtZero) {
	Player& gm = online(730100, "Warden", 3);
	handlers::admincommands::AddExp addExp;
	EXPECT_TRUE(addExp.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(addExp.getSyntaxInfo()));

	const int64_t before = gm.getCommonData()->getExp();
	client()->clearSent();
	EXPECT_TRUE(addExp.process(gm, args({"500"})));
	EXPECT_EQ(gm.getCommonData()->getExp(), before + 500);
	EXPECT_TRUE(sent(info("You added 500 exp points to [charname:Warden;1 1 1].")[0]));

	client()->clearSent();
	EXPECT_TRUE(addExp.process(gm, args({"-999999"})));
	EXPECT_EQ(gm.getCommonData()->getExp(), 0) << "Math.max(0, ...)";
	EXPECT_TRUE(sent(info("You added -999999 exp points to [charname:Warden;1 1 1].")[0]));
}

TEST_F(CharacterCommandsTest, SetLevelExpAndTheUnknownArm) {
	Player& gm = online(730101, "Warden", 3);
	handlers::admincommands::Set set;
	EXPECT_TRUE(set.process(gm, args({"level"})));
	EXPECT_EQ(client()->sentBytes(), info(set.getSyntaxInfo())) << "fewer than two parameters";

	client()->clearSent();
	EXPECT_TRUE(set.process(gm, args({"level", "5"})));
	EXPECT_EQ(gm.getLevel(), 5);
	EXPECT_TRUE(sent(info("Set [charname:Warden;1 1 1]'s level to 5")[0]));

	client()->clearSent();
	EXPECT_TRUE(set.process(gm, args({"level", "99"})));
	EXPECT_EQ(gm.getLevel(), 9) << "min(PLAYER_MAX_LEVEL 15, 99); a non-daeva stops at level 9 (PlayerCommonData.setExp)";

	client()->clearSent();
	EXPECT_TRUE(set.process(gm, args({"exp", "1433"})));
	EXPECT_EQ(gm.getCommonData()->getExp(), 1433);
	EXPECT_TRUE(sent(info("Set exp of target to 1433")[0]));

	client()->clearSent();
	EXPECT_TRUE(set.process(gm, args({"LEVEL", "3"})));
	EXPECT_EQ(client()->sentBytes(), info(set.getSyntaxInfo())) << "the arms are case-sensitive (String.equals)";
}

TEST_F(CharacterCommandsTest, LevelupAndLeveldownStayInsideTheLevels) {
	Player& gm = online(730102, "Warden", 3);
	gm.getCommonData()->setLevel(5);
	handlers::consolecommands::Levelup levelup;
	handlers::consolecommands::Leveldown leveldown;
	client()->clearSent();
	EXPECT_TRUE(levelup.process(gm, args({"2"})));
	EXPECT_EQ(gm.getLevel(), 7);
	EXPECT_TRUE(sent(info("Set [charname:Warden;1 1 1]'s level to 7")[0]));

	client()->clearSent();
	EXPECT_TRUE(leveldown.process(gm, args({"3"})));
	EXPECT_EQ(gm.getLevel(), 4);

	client()->clearSent();
	EXPECT_TRUE(leveldown.process(gm, args({"4"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid level.")) << "level 0";
	client()->clearSent();
	EXPECT_TRUE(levelup.process(gm, args({"12"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid level.")) << "above PLAYER_MAX_LEVEL";
	client()->clearSent();
	EXPECT_TRUE(levelup.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(levelup.getSyntaxInfo()));
	EXPECT_EQ(gm.getLevel(), 4);
}

// ---- //addskill, //delskill ---------------------------------------------------------------------------------------------------------------

TEST_F(CharacterCommandsTest, AddSkillAndDelSkillRefusals) {
	Player& gm = connected(730103, "Warden", 3);
	handlers::admincommands::AddSkill addSkill;
	EXPECT_TRUE(addSkill.process(gm, args({"1"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax //addskill <skillId> <skillLevel>")})) << "sendMessage, not sendInfo";
	client()->clearSent();
	EXPECT_TRUE(addSkill.process(gm, args({"1", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Parameters need to be an integer.")}));

	handlers::admincommands::DelSkill delSkill;
	client()->clearSent();
	EXPECT_TRUE(delSkill.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(),
		exactly({message("No parameters detected.\nPlease use //delskill <Player name> <all | skillId>\nor use //delskill [target] <all | skillId>")}));
	client()->clearSent();
	EXPECT_TRUE(delSkill.process(gm, args({"Nobody", "5"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("The specified player is not online.")}));
	client()->clearSent();
	EXPECT_TRUE(delSkill.process(gm, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Param 0 must be an integer or <all>.")}));
	client()->clearSent();
	EXPECT_TRUE(delSkill.process(gm, args({"9999"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player dont have this skill.")}));
	client()->clearSent();
	EXPECT_TRUE(delSkill.process(gm, args({"0"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("You have success delete All skills.")}))
		<< "the owner's correction of 2026-10-05: skill ID 0 deletes all (Java threw getSkillEntry(0).isStigmaSkill()'s NullPointerException)";
}

// ---- //heal ---------------------------------------------------------------------------------------------------------------------------------

TEST_F(CharacterCommandsTest, HealArms) {
	Player& gm = connected(730104, "Warden", 3);
	handlers::admincommands::Heal heal;
	EXPECT_TRUE(heal.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(heal.getSyntaxInfo())) << "no target";

	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&gm));
	for (const char* arm : {"%", "x%"}) {
		client()->clearSent();
		EXPECT_TRUE(heal.process(gm, args({arm})));
		EXPECT_EQ(client()->sentBytes(), info("Invalid number.")) << arm << ": Integer.parseInt(CharSequence, ...) has no \"For input string\" message";
	}
	client()->clearSent();
	EXPECT_TRUE(heal.process(gm, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"abc\""));

	client()->clearSent();
	EXPECT_TRUE(heal.process(gm, args({"fp"})));
	EXPECT_EQ(gm.getLifeStats()->getCurrentFp(), gm.getLifeStats()->getMaxFp());
	EXPECT_FALSE(sent(info("[charname:Warden;1 1 1]'s flight time has been fully refreshed.")[0])) << "no message when healing oneself";
}

// ---- //speed, //stat ------------------------------------------------------------------------------------------------------------------------

TEST_F(CharacterCommandsTest, SpeedFixesAndRestoresTheSpeed) {
	Player& gm = connected(730105, "Warden", 3);
	// the command owns the stat functions (Java: Speed implements StatOwner); a registered command is immortal, and the functions' proxies
	// release their owner when the reclamation at the end of the task scope frees them, after this body
	static auto* const registered = new handlers::admincommands::Speed();
	handlers::admincommands::Speed& speed = *registered;
	const int32_t regular = gm.getGameStats()->getMovementSpeed()->getCurrent();
	EXPECT_TRUE(speed.process(gm, args({"101"})));
	EXPECT_EQ(client()->sentBytes(), info("Speed must be between 0 and 100."));

	client()->clearSent();
	EXPECT_TRUE(speed.process(gm, args({"12.5"})));
	EXPECT_EQ(gm.getGameStats()->getStat(StatEnum::SPEED, 0)->getCurrent(), 12500);
	EXPECT_EQ(gm.getGameStats()->getStat(StatEnum::FLY_SPEED, 0)->getCurrent(), 12500);
	EXPECT_TRUE(sent(info("Your speed is now fixed at 12.5.")[0]));

	client()->clearSent();
	EXPECT_TRUE(speed.process(gm, args({"0"})));
	EXPECT_EQ(gm.getGameStats()->getMovementSpeed()->getCurrent(), regular);
	EXPECT_TRUE(sent(info("Your regular speed has been restored.")[0]));

	client()->clearSent();
	EXPECT_TRUE(speed.process(gm, args({"fast"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"fast\"")) << "Float.parseFloat";
}

TEST_F(CharacterCommandsTest, StatSetsShowsAndCancels) {
	Player& gm = connected(730106, "Warden", 3);
	handlers::admincommands::Stat stat;
	const std::string white = utils::ChatUtil::color("SPEED", utils::JavaColor::WHITE);
	EXPECT_TRUE(stat.process(gm, args({"speed", "7000"})));
	EXPECT_EQ(gm.getGameStats()->getStat(StatEnum::SPEED, 0)->getCurrent(), 7000);
	EXPECT_TRUE(sent(info("Your " + white + " is now set to 7000.")[0]));

	client()->clearSent();
	EXPECT_TRUE(stat.process(gm, args({"speed"})));
	const std::vector<std::vector<uint8_t>> shown = client()->sentBytes();
	ASSERT_GE(shown.size(), 2u);
	EXPECT_TRUE(sent(info(utils::ChatUtil::leftPad(1, 3) + "x " + utils::ChatUtil::color("=7000", utils::JavaColor::CYAN) +
						  ", priority: 120, type: CommandStatFunction, owner: CommandStatOwner")[0]));

	client()->clearSent();
	EXPECT_TRUE(stat.process(gm, args({"cancel"})));
	EXPECT_TRUE(sent(info("Your stat overrides have been canceled.")[0])) << "after the stats packets of onStatsChange";
	EXPECT_NE(gm.getGameStats()->getStat(StatEnum::SPEED, 0)->getCurrent(), 7000);

	client()->clearSent();
	EXPECT_TRUE(stat.process(gm, args({"s"})));
	EXPECT_EQ(client()->sentBytes(), info("There is no stat with that name.")) << "shorter than two characters";
	client()->clearSent();
	EXPECT_TRUE(stat.process(gm, args({"flytime"})));
	EXPECT_TRUE(client()->sentBytes().size() >= 1);
	EXPECT_NE(client()->sentBytes(), info("There is no stat with that name.")) << "FLY_TIME matched without its underscore";
	client()->clearSent();
	EXPECT_TRUE(stat.process(gm, args({"abs", "1"})));
	EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>"))
		<< "ABSOLUTE_STATS_DATA is not published here: the null holder is caught as Java catches its NPE";
}

// ---- //dispel, //morph, //state ------------------------------------------------------------------------------------------------------------

TEST_F(CharacterCommandsTest, DispelNamesTheTargetByToString) {
	Player& gm = connected(730107, "Warden", 3);
	handlers::admincommands::Dispel dispel;
	EXPECT_TRUE(dispel.process(gm, args({})));
	EXPECT_TRUE(sent(info("Removed all effects of " + gm.toString() + ".")[0]));
}

TEST_F(CharacterCommandsTest, MorphWithoutATargetAndTheCancel) {
	Player& gm = connected(730108, "Warden", 3);
	handlers::admincommands::Morph morph;
	EXPECT_TRUE(morph.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(morph.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(morph.process(gm, args({"0"})));
	EXPECT_TRUE(sent(info("Cancelled morph.")[0]));
}

TEST_F(CharacterCommandsTest, StateSetsAddsAndRemoves) {
	Player& gm = connected(730109, "Warden", 3);
	handlers::admincommands::State state;
	EXPECT_TRUE(state.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), info(state.getSyntaxInfo())) << "no target";

	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&gm));
	client()->clearSent();
	EXPECT_TRUE(state.process(gm, args({"active"})));
	EXPECT_EQ(gm.getState(), 1);
	EXPECT_TRUE(sent(info("[charname:Warden;1 1 1]'s state changed to 1 = ACTIVE (1)")[0]));

	client()->clearSent();
	EXPECT_TRUE(state.process(gm, args({"add", "4"})));
	EXPECT_EQ(gm.getState(), 5);
	EXPECT_TRUE(sent(info("[charname:Warden;1 1 1]'s state changed to 5 = ACTIVE (1) + RESTING (4)")[0]));

	client()->clearSent();
	EXPECT_TRUE(state.process(gm, args({"remove", "ACTIVE"})));
	EXPECT_EQ(gm.getState(), 4);

	client()->clearSent();
	EXPECT_TRUE(state.process(gm, args({"remove", "-1"})));
	EXPECT_EQ(client()->sentBytes(), info("Out of range state ID.")) << "the syntax's -1 is refused (proposed correction)";
	client()->clearSent();
	EXPECT_TRUE(state.process(gm, args({"add"})));
	EXPECT_EQ(client()->sentBytes(), info("Please provide a state name or ID."));
	client()->clearSent();
	EXPECT_TRUE(state.process(gm, args({"sleepy"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"sleepy\""));
	EXPECT_EQ(gm.getState(), 4);
}

// ---- //removecd, ///clearusercoolt --------------------------------------------------------------------------------------------------------

TEST_F(CharacterCommandsTest, RemoveCdAndClearusercoolt) {
	Player& gm = online(730110, "Warden", 3);
	handlers::admincommands::RemoveCd removeCd;
	EXPECT_TRUE(removeCd.process(gm, args({})));
	EXPECT_TRUE(sent(info("Your item and skill cooldowns were removed.")[0]));

	client()->clearSent();
	EXPECT_TRUE(removeCd.process(gm, args({"INSTANCE", "all"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("You have no instance cooldowns to remove.")})) << "Clearusercoolt's sendMessage";

	client()->clearSent();
	EXPECT_TRUE(removeCd.process(gm, args({"instance", "1234"})));
	EXPECT_EQ(client()->sentBytes(), info("You have no cooldown on 1234.")) << "an unknown world is named by its ID";

	client()->clearSent();
	EXPECT_TRUE(removeCd.process(gm, args({"instance"})));
	EXPECT_EQ(client()->sentBytes(), info(removeCd.getSyntaxInfo()));

	handlers::consolecommands::Clearusercoolt clear;
	client()->clearSent();
	EXPECT_TRUE(clear.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"), client().con())}));
	client()->clearSent();
	EXPECT_TRUE(clear.process(gm, args({"warden"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("You have no instance cooldowns to remove.")})) << "the name is normalized";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
