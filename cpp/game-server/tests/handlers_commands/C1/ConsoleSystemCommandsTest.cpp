// M5j stage 4 CP4 (m5j-plan.md §18.5, L-01): the console remainder (data/handlers/consolecommands) on real Players with real AionConnections
// (CommandTestSupport.h) in a Poeta map instance. The texts are the Java literals. The commands that delegate to an admin command
// (Attrbonus -> Stat, Deletecquest -> Quest) find it through ChatProcessor.getCommand(Class), so the fixture registers those once per process
// (immortal, like ChatProcessor.init's commands); the expected output of a delegation is the admin command's own output for the same call.

#include "CommandTestSupport.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/handlers/admincommands/Quest.h"
#include "aion/gameserver/handlers/admincommands/Set.h"
#include "aion/gameserver/handlers/admincommands/Stat.h"
#include "aion/gameserver/handlers/consolecommands/Attrbonus.h"
#include "aion/gameserver/handlers/consolecommands/Delete_items.h"
#include "aion/gameserver/handlers/consolecommands/Deletecquest.h"
#include "aion/gameserver/handlers/consolecommands/Endquest.h"
#include "aion/gameserver/handlers/consolecommands/Givetitle.h"
#include "aion/gameserver/handlers/consolecommands/Guild.h"
#include "aion/gameserver/handlers/consolecommands/Inventory.h"
#include "aion/gameserver/handlers/consolecommands/Invisible.h"
#include "aion/gameserver/handlers/consolecommands/Itemcooltime.h"
#include "aion/gameserver/handlers/consolecommands/Partyrecall.h"
#include "aion/gameserver/handlers/consolecommands/Remove_skill_delay_all.h"
#include "aion/gameserver/handlers/consolecommands/Resurrect.h"
#include "aion/gameserver/handlers/consolecommands/Search.h"
#include "aion/gameserver/handlers/consolecommands/Set_makeup_bonus.h"
#include "aion/gameserver/handlers/consolecommands/Set_vitalpoint.h"
#include "aion/gameserver/handlers/consolecommands/Skill.h"
#include "aion/gameserver/handlers/consolecommands/Status.h"
#include "aion/gameserver/handlers/consolecommands/Visible.h"
#include "aion/gameserver/handlers/consolecommands/Wishid.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SEARCH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_SKILLS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::state::CreatureVisualState;
using serverpackets::SM_SYSTEM_MESSAGE;

/** Stat, Quest and Set in ChatProcessor, once per process (immortal) */
void registerDelegates() {
	static const bool registered = [] {
		utils::chathandlers::ChatProcessor& processor = utils::chathandlers::ChatProcessor::getInstance();
		if (processor.getCommand<handlers::admincommands::Stat>() == nullptr)
			processor.registerCommand(*new handlers::admincommands::Stat());
		if (processor.getCommand<handlers::admincommands::Quest>() == nullptr)
			processor.registerCommand(*new handlers::admincommands::Quest());
		if (processor.getCommand<handlers::admincommands::Set>() == nullptr)
			processor.registerCommand(*new handlers::admincommands::Set());
		return true;
	}();
	static_cast<void>(registered);
}

class ConsoleSystemCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		// commands.properties' console levels; givetitle at 1 so a level-1 GM reaches its own "set" check
		for (const char* alias : {"attrbonus", "deletecquest", "guild", "inventory", "invisible", "visible", "itemcooltime", "partyrecall",
				 "remove_skill_delay_all", "resurrect", "search", "skill", "status", "endquest", "delete_items", "set_makeup_bonus",
				 "set_vitalpoint", "wishid", "quest"})
			levels[alias] = 3;
		levels["givetitle"] = 1;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		for (PlayerFixture& fixture : players)
			fixture.player->setTarget(nullptr);
		stored.clear();
		CommandTest::TearDown();
		if (itemsPublished)
			dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	Player& gm(int32_t objectId, std::string_view name = "Warden", int8_t accessLevel = 9) {
		Player& player = connected(objectId, name, accessLevel);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	void target(Player& player, model::gameobjects::VisibleObject& object) {
		player.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&object));
		clearAll();
	}

	void clearAll() {
		for (size_t i = 0; i < clients.size(); i++)
			client(i)->clearSent();
	}

	template <class P>
	std::vector<uint8_t> packet(P&& p, size_t index = 0) {
		return serialized(std::forward<P>(p), client(index).con());
	}

	xml::LoadContext context;
	std::vector<Player*> stored;
	bool itemsPublished = false;
};

// ---- the trivial ones -------------------------------------------------------------------------------------------------------------------

/** Inventory.java:15-16 (the body returns at once), Partyrecall.java:17-20 */
TEST_F(ConsoleSystemCommandsTest, InventoryAndPartyrecall) {
	Player& admin = gm(731201);
	handlers::consolecommands::Inventory inventory;
	EXPECT_EQ(inventory.getAlias(), "inventory");
	EXPECT_TRUE(inventory.process(admin, args({"x"})));
	EXPECT_TRUE(client()->sentBytes().empty());
	handlers::consolecommands::Partyrecall partyrecall;
	EXPECT_TRUE(partyrecall.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Command not implemented.")});
}

/** Remove_skill_delay_all.java:20-33: "0" disables the cooldowns once, "1" restores them once; anything else is silent */
TEST_F(ConsoleSystemCommandsTest, RemoveSkillDelayAll) {
	Player& admin = gm(731202);
	handlers::consolecommands::Remove_skill_delay_all command;
	EXPECT_TRUE(command.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	clearAll();
	EXPECT_TRUE(command.process(admin, args({"1"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "not in the mode";
	EXPECT_TRUE(command.process(admin, args({"0"})));
	EXPECT_EQ(client()->sentBytes(), info("Cooldown times of all skills have been disabled."));
	EXPECT_TRUE(admin.isInCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE));
	clearAll();
	EXPECT_TRUE(command.process(admin, args({"0"})));
	EXPECT_TRUE(command.process(admin, args({"01"})));
	EXPECT_TRUE(client()->sentBytes().empty());
	EXPECT_TRUE(command.process(admin, args({"1"})));
	EXPECT_EQ(client()->sentBytes(), info("Cooldown times of all skills have been recovered."));
	EXPECT_FALSE(admin.isInCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE));
}

/** Invisible.java:24-34, Visible.java:24-34: the state changes once, the system message and SM_ABNORMAL_STATE every time */
TEST_F(ConsoleSystemCommandsTest, InvisibleAndVisible) {
	Player& admin = gm(731203);
	handlers::consolecommands::Invisible invisible;
	handlers::consolecommands::Visible visible;
	EXPECT_TRUE(visible.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), (std::vector<std::vector<uint8_t>>{packet(SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_END()),
		packet(serverpackets::SM_ABNORMAL_STATE({}, admin.getEffectController()->getAbnormals(), 0))})) << "visible already: no state change";
	clearAll();
	EXPECT_TRUE(invisible.process(admin, args({})));
	EXPECT_TRUE(admin.isInVisualState(CreatureVisualState::HIDE20));
	std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_GE(sent.size(), 3u);
	EXPECT_EQ(sent[sent.size() - 3], packet(serverpackets::SM_PLAYER_STATE(admin))) << "broadcast to the player himself";
	EXPECT_EQ(sent[sent.size() - 2], packet(SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_BEGIN()));
	EXPECT_EQ(sent.back(), packet(serverpackets::SM_ABNORMAL_STATE({}, admin.getEffectController()->getAbnormals(), 0)));
	EXPECT_NE(admin.getEffectController()->getAbnormals(), 0) << "AbnormalState.HIDE";
	clearAll();
	EXPECT_TRUE(invisible.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), (std::vector<std::vector<uint8_t>>{packet(SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_BEGIN()),
		packet(serverpackets::SM_ABNORMAL_STATE({}, admin.getEffectController()->getAbnormals(), 0))})) << "hidden already";
	clearAll();
	EXPECT_TRUE(visible.process(admin, args({})));
	EXPECT_FALSE(admin.isInVisualState(CreatureVisualState::HIDE20));
	EXPECT_EQ(admin.getEffectController()->getAbnormals(), 0);
	sent = client()->sentBytes();
	ASSERT_GE(sent.size(), 3u);
	EXPECT_EQ(sent[sent.size() - 3], packet(serverpackets::SM_PLAYER_STATE(admin)));
	EXPECT_EQ(sent[sent.size() - 2], packet(SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_END()));
}

// ---- the player lookups -----------------------------------------------------------------------------------------------------------------

/** Search.java:19-25, Status.java:19-30, Skill.java:23-34: by name, else (Status, Skill) the targeted player; nothing for nobody */
TEST_F(ConsoleSystemCommandsTest, SearchStatusAndSkill) {
	Player& admin = gm(731204);
	Player& other = gm(731205, "Rover", 0);
	handlers::consolecommands::Search search;
	handlers::consolecommands::Status status;
	handlers::consolecommands::Skill skill;
	for (utils::chathandlers::ChatCommand* command : std::vector<utils::chathandlers::ChatCommand*>{&search, &status, &skill}) {
		clearAll();
		EXPECT_TRUE(command->process(admin, args({})));
		EXPECT_TRUE(command->process(admin, args({"Nobody"})));
		EXPECT_TRUE(client()->sentBytes().empty()) << command->getAlias();
	}
	EXPECT_TRUE(search.process(admin, args({"Rover"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_GM_SEARCH(other))});
	clearAll();
	EXPECT_TRUE(status.process(admin, args({"Rover"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_GM_SHOW_PLAYER_STATUS(other))});
	clearAll();
	EXPECT_TRUE(skill.process(admin, args({"Rover"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "no skills: DynamicServerPacketBodySplitList(..., false, ...) has no part";
	other.setSkillList(model::skill::PlayerSkillList::create({
		model::skill::PlayerSkillEntry::create(30002, 20, 0, model::gameobjects::Persistable_PersistentState::NOACTION),
		model::skill::PlayerSkillEntry::create(42, 1, 0, model::gameobjects::Persistable_PersistentState::NOACTION)}));
	EXPECT_TRUE(skill.process(admin, args({"Rover"})));
	std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>> skills;
	for (runtime::Ptr<model::skill::PlayerSkillEntry> entry : other.getSkillList()->getAllSkills())
		skills.push_back(entry);
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_GM_SHOW_PLAYER_SKILLS(skills))}) << "one part";

	target(admin, other);
	EXPECT_TRUE(search.process(admin, args({})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "Search has no target arm";
	EXPECT_TRUE(status.process(admin, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_GM_SHOW_PLAYER_STATUS(other))}) << "the target";
	clearAll();
	EXPECT_TRUE(skill.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_GM_SHOW_PLAYER_SKILLS(skills))}) << "the target";
	clearAll();
	EXPECT_TRUE(status.process(admin, args({"Warden"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_GM_SHOW_PLAYER_STATUS(admin))}) << "the name first";
}

/** Guild.java:24-35: nothing for an unknown player or one without a legion */
TEST_F(ConsoleSystemCommandsTest, GuildWithoutLegion) {
	Player& admin = gm(731206);
	gm(731207, "Rover", 0);
	handlers::consolecommands::Guild guild;
	for (std::vector<std::string> params : {args({}), args({"Nobody"}), args({"Rover"})}) {
		EXPECT_TRUE(guild.process(admin, params));
		EXPECT_TRUE(client()->sentBytes().empty()) << params.size();
	}
}

/** Resurrect.java:18-31: the dead GM or the targeted dead player gets SM_RESURRECT and the res flag */
TEST_F(ConsoleSystemCommandsTest, Resurrect) {
	Player& admin = gm(731208);
	Player& other = gm(731209, "Rover", 0);
	handlers::consolecommands::Resurrect resurrect;
	EXPECT_TRUE(resurrect.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Please select a player."));
	target(admin, other);
	EXPECT_TRUE(resurrect.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Rover is already alive."));
	target(admin, admin);
	EXPECT_TRUE(resurrect.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info("You're already alive."));

	target(admin, other);
	other.setLifeStats(std::make_unique<DeadPlayerLifeStats>(other));
	EXPECT_TRUE(resurrect.process(admin, args({})));
	EXPECT_TRUE(client()->sentBytes().empty());
	EXPECT_EQ(client(1)->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_RESURRECT(admin), 1)});
	EXPECT_TRUE(other.getResStatus());

	clearAll();
	admin.setLifeStats(std::make_unique<DeadPlayerLifeStats>(admin));
	EXPECT_TRUE(resurrect.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_RESURRECT(admin))}) << "a dead GM himself";
	EXPECT_TRUE(admin.getResStatus());
	EXPECT_TRUE(client(1)->sentBytes().empty());
}

// ---- the stat setters ---------------------------------------------------------------------------------------------------------------------

/** Set_makeup_bonus.java:18-38, Set_vitalpoint.java:18-37: the target (else the GM) gets the value and SM_STATS_INFO */
TEST_F(ConsoleSystemCommandsTest, SetMakeupBonusAndVitalpoint) {
	Player& admin = gm(731210);
	Player& other = gm(731211, "Rover", 0);
	handlers::consolecommands::Set_makeup_bonus makeup;
	handlers::consolecommands::Set_vitalpoint vital;
	for (std::vector<std::string> params : {args({}), args({"x"})}) {
		clearAll();
		EXPECT_TRUE(makeup.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///set_makeup_bonus <value>")});
		clearAll();
		EXPECT_TRUE(vital.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///set_vitalpoint <value>")});
	}
	clearAll();
	EXPECT_TRUE(makeup.process(admin, args({"5000000000"})));
	EXPECT_EQ(admin.getCommonData()->getCurrentReposeEnergy(), 5000000000LL) << "Long.parseLong";
	EXPECT_EQ(client()->sentBytes(), (std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_STATS_INFO(admin)), message("Repose Energy set to: 5000000000")}));
	target(admin, other);
	EXPECT_TRUE(makeup.process(admin, args({"123"})));
	EXPECT_EQ(other.getCommonData()->getCurrentReposeEnergy(), 123) << "the target";
	EXPECT_EQ(admin.getCommonData()->getCurrentReposeEnergy(), 5000000000LL);
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Repose Energy set to: 123")});
	EXPECT_EQ(client(1)->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_STATS_INFO(other), 1)});
	clearAll();
	EXPECT_TRUE(vital.process(admin, args({"7700"})));
	EXPECT_EQ(other.getCommonData()->getCurrentSalvationPercent(), 7) << "7700 points: 7 %";
	EXPECT_TRUE(client()->sentBytes().empty());
	EXPECT_EQ(client(1)->sentBytes(), std::vector<std::vector<uint8_t>>{packet(serverpackets::SM_STATS_INFO(other), 1)});
	clearAll();
	EXPECT_TRUE(vital.process(admin, args({"5000000000"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///set_vitalpoint <value>")}) << "Integer.parseInt";
}

// ---- the delegations ------------------------------------------------------------------------------------------------------------------------

/** Attrbonus.java:25-38: each arm is Stat's own method on the GM */
TEST_F(ConsoleSystemCommandsTest, AttrbonusDelegatesToStat) {
	registerDelegates();
	Player& admin = gm(731212);
	auto* stat = utils::chathandlers::ChatProcessor::getInstance().getCommand<handlers::admincommands::Stat>();
	ASSERT_NE(stat, nullptr);
	handlers::consolecommands::Attrbonus attrbonus;
	const auto expectSame = [&](std::vector<std::string> params, const std::function<void()>& oracle) {
		clearAll();
		EXPECT_TRUE(attrbonus.process(admin, params));
		const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
		clearAll();
		oracle();
		EXPECT_EQ(sent, client()->sentBytes()) << params[0];
		EXPECT_FALSE(sent.empty()) << params[0];
	};
	expectSame(args({"LIST"}), [&] { stat->listStats(admin); });
	expectSame(args({"maxhp"}), [&] { stat->showStatFunctions(admin, admin, "maxhp"); });
	// the state-changing arms: Stat's call runs from the same state (the override undone in between)
	clearAll();
	EXPECT_TRUE(attrbonus.process(admin, args({"parry", "500"})));
	const std::vector<std::vector<uint8_t>> set = client()->sentBytes();
	stat->cancelStatOverrides(admin, admin);
	clearAll();
	stat->setStat(admin, admin, "parry", 500);
	// the stat packets that follow an effect change depend on what was applied before; the closing text is Stat's
	ASSERT_FALSE(set.empty());
	EXPECT_EQ(set.back(), client()->sentBytes().back()) << "parry 500";
	clearAll();
	EXPECT_TRUE(attrbonus.process(admin, args({"cancel"})));
	const std::vector<std::vector<uint8_t>> cancel = client()->sentBytes();
	stat->setStat(admin, admin, "parry", 500);
	clearAll();
	stat->cancelStatOverrides(admin, admin);
	ASSERT_FALSE(cancel.empty());
	EXPECT_EQ(cancel.back(), client()->sentBytes().back()) << "cancel";
	EXPECT_NE(set.back(), cancel.back());
	clearAll();
	EXPECT_TRUE(attrbonus.process(admin, args({"a", "b", "c"})));
	EXPECT_EQ(client()->sentBytes(), info(attrbonus.getSyntaxInfo()));
	clearAll();
	EXPECT_TRUE(attrbonus.process(admin, args({"maxhp", "lots"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"lots\"")) << "Integer.parseInt";
}

/** Deletecquest.java:22-30: Quest.deleteQuest on the targeted player, else on the GM */
TEST_F(ConsoleSystemCommandsTest, DeletecquestDelegatesToQuest) {
	registerDelegates();
	Player& admin = gm(731213);
	Player& other = gm(731214, "Rover", 0);
	auto* quest = utils::chathandlers::ChatProcessor::getInstance().getCommand<handlers::admincommands::Quest>();
	ASSERT_NE(quest, nullptr);
	handlers::consolecommands::Deletecquest deletecquest;
	EXPECT_TRUE(deletecquest.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(deletecquest.getSyntaxInfo()));
	const auto expectSame = [&](const std::function<void()>& oracle) {
		clearAll();
		EXPECT_TRUE(deletecquest.process(admin, args({"1001"})));
		const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
		clearAll();
		oracle();
		EXPECT_EQ(sent, client()->sentBytes());
		EXPECT_FALSE(sent.empty());
	};
	expectSame([&] { quest->deleteQuest(admin, admin, 1001); });
	target(admin, other);
	expectSame([&] { quest->deleteQuest(admin, other, 1001); });
}

/** Givetitle.java:17-56: the syntax (Java's ///addskill text) and the //set check. ChatProcessor keys commands by their alias with prefix
 * ("//set"), and Java asks isCommandAllowed(admin, "set"): every GM is refused, with //set registered or not (J-S4CP4-3) */
TEST_F(ConsoleSystemCommandsTest, GivetitleIsAlwaysRefused) {
	Player& admin = gm(731215);
	handlers::consolecommands::Givetitle givetitle;
	EXPECT_TRUE(givetitle.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///addskill <named name>")});
	for (bool registered : {false, true}) {
		if (registered)
			registerDelegates();
		ASSERT_FALSE(utils::chathandlers::ChatProcessor::getInstance().isCommandExists("set")) << "the key has the prefix";
		clearAll();
		EXPECT_TRUE(givetitle.process(admin, args({"4"})));
		EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("You dont have enough rights to execute this command")}) << registered;
	}
	EXPECT_TRUE(utils::chathandlers::ChatProcessor::getInstance().isCommandExists("//set"));
	EXPECT_FALSE(admin.getTitleList().contains(4));
}

// ---- the quest, item and wish commands, up to their services --------------------------------------------------------------------------------

/** Endquest.java:24-50: the syntax, an invalid quest, a quest not started (the completion itself is //quest's, CP6) */
TEST_F(ConsoleSystemCommandsTest, EndquestRefusals) {
	Player& admin = gm(731217);
	handlers::consolecommands::Endquest endquest;
	EXPECT_TRUE(endquest.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(endquest.getSyntaxInfo()));
	clearAll();
	EXPECT_TRUE(endquest.process(admin, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid quest link or ID."));
	clearAll();
	EXPECT_TRUE(endquest.process(admin, args({"1001"})));
	EXPECT_EQ(client()->sentBytes(), info("Quest must be started first."));
}

/** Delete_items.java:18-46: the syntax, the integer check, the quality range (ItemQuality has 7 constants); an empty inventory loses nothing */
TEST_F(ConsoleSystemCommandsTest, DeleteItemsRefusals) {
	Player& admin = gm(731218);
	handlers::consolecommands::Delete_items deleteItems;
	EXPECT_TRUE(deleteItems.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///delete_items <item quality>")});
	for (const char* quality : {"-1", "7"}) {
		clearAll();
		EXPECT_TRUE(deleteItems.process(admin, args({quality})));
		EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Invalid QualityId.")}) << quality;
	}
	clearAll();
	EXPECT_TRUE(deleteItems.process(admin, args({"x"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Parameters need to be an integer.")});
	clearAll();
	EXPECT_TRUE(deleteItems.process(admin, args({"6"})));
	EXPECT_TRUE(client()->sentBytes().empty());
}

/** Wishid.java:18-55: the syntax (fewer than two parameters or no numbers) and an unknown item id */
TEST_F(ConsoleSystemCommandsTest, WishidRefusals) {
	Player& admin = gm(731219);
	handlers::consolecommands::Wishid wishid;
	for (std::vector<std::string> params : {args({}), args({"1"}), args({"x", "1"}), args({"1", "x"})}) {
		clearAll();
		EXPECT_TRUE(wishid.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("syntax ///wishid <Quantity> <item Id>")}) << params.size();
	}
	ASSERT_FALSE(dataholders::DataManager::ITEM_DATA) << "published by an earlier case of this process";
	dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, "<item_templates/>"));
	itemsPublished = true;
	clearAll();
	EXPECT_TRUE(wishid.process(admin, args({"2", "999999999"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{message("Item id is incorrect: 999999999")});
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
