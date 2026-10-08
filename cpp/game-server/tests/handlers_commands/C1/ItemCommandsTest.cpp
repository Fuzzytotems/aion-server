// The stage-2 item commands (m5j-plan.md §5.4, §18.3 stage 2 CP4, item E-07): //add, //addset, //addcube, //remove, //equip, //dye,
// //megaphone, //rename, //res, .preview, .nomorph, .noexp, .del, .decompose. Real Players with real AionConnections (CommandTestSupport.h),
// online in the World; the item rows are ItemPacketTestSupport.h's plus the megaphone, emotion card, paint and decomposable rows below (copied
// from the shipped data, file:line beside each). This executable has no database: //rename is driven up to its name checks. The texts are the
// Java literals of the command files.

#include "CommandTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.bind.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/EnchantData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/handlers/admincommands/Add.h"
#include "aion/gameserver/handlers/admincommands/AddCube.h"
#include "aion/gameserver/handlers/admincommands/AddSet.h"
#include "aion/gameserver/handlers/admincommands/Dye.h"
#include "aion/gameserver/handlers/admincommands/Equip.h"
#include "aion/gameserver/handlers/admincommands/Megaphone.h"
#include "aion/gameserver/handlers/admincommands/Remove.h"
#include "aion/gameserver/handlers/admincommands/Rename.h"
#include "aion/gameserver/handlers/admincommands/Res.h"
#include "aion/gameserver/handlers/playercommands/Decompose.h"
#include "aion/gameserver/handlers/playercommands/Del.h"
#include "aion/gameserver/handlers/playercommands/NoExp.h"
#include "aion/gameserver/handlers/playercommands/NoMorph.h"
#include "aion/gameserver/handlers/playercommands/Preview.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MEGAPHONE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;
using namespace items; // the item ids and helpers of ItemPacketTestSupport.h

inline constexpr int32_t MINT_MEGAPHONE = 188910002;
inline constexpr int32_t AZURE_MEGAPHONE = 188910003;
inline constexpr int32_t AION_BOOGIE_CARD = 169600001;
inline constexpr int32_t PAINT_RED = 169120000;
inline constexpr int32_t PEPENTO = 152000064;
inline constexpr int32_t JUICY_PEPENTO = 152000065;

/** item_templates.xml:930373-930384 (the megaphones), :855718-855722 (the emotion card), :848746-848750 (the paint), :743602-743608 (Pepento) */
inline constexpr std::string_view EXTRA_ITEM_TEMPLATES_XML = R"xml(
	<item_template id="188910002" name="Mint Megaphone" level="1" cName="world_cash_item_megaphone_10a" mask="12410" max_stack_count="1000" quality="RARE" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="822113" activate_target="STANDALONE" activate_count="1">
		<actions>
			<megaphone color="caf264"/>
		</actions>
		<uselimits usedelay="10000" usedelayid="146"/>
	</item_template>
	<item_template id="188910003" name="Azure Megaphone" level="1" cName="world_cash_item_megaphone_10b" mask="12410" max_stack_count="1000" quality="RARE" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="822114" activate_target="STANDALONE" activate_count="1">
		<actions>
			<megaphone color="73eeee"/>
		</actions>
		<uselimits usedelay="10000" usedelayid="146"/>
	</item_template>
	<item_template id="169600001" name="[Emotion Card] 'Aion Boogie'" level="1" cName="cash_add_social_cash_danceA_01" mask="4168" quality="COMMON" price="0" desc="725539" activate_count="1">
		<actions>
			<learnemotion emotionid="64"/>
		</actions>
	</item_template>
	<item_template id="169120000" name="Paint: Red" level="1" cName="paint_red_01" mask="12414" max_stack_count="100" quality="COMMON" price="5" desc="797431" activate_count="1">
		<actions>
			<dye color="c22626"/>
		</actions>
	</item_template>
	<item_template id="152000064" name="Pepento" level="60" cName="vegetable_C_60a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="420" desc="811631"/>
	<item_template id="152000065" name="Juicy Pepento" level="60" cName="vegetable_R_60a" casting_delay="3000" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="RARE" price="1260" desc="811632" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelayid="85"/>
	</item_template>
</item_templates>)xml";

/** decomposable_items.xml:3-7 */
inline constexpr std::string_view DECOMPOSABLE_ITEMS_XML = R"xml(<decomposable_items>
	<decomposable item_id="152000065">
		<items>
			<item id="152000064" min_count="2" />
		</items>
	</decomposable>
</decomposable_items>)xml";

/** whether the packet's bytes hold the text as UTF-16LE (an SM_MESSAGE body writes its text with writeS) */
bool holdsText(const std::vector<uint8_t>& bytes, std::string_view text) {
	std::vector<uint8_t> needle;
	for (char c : text) {
		needle.push_back(static_cast<uint8_t>(c));
		needle.push_back(0);
	}
	return std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end();
}

class ItemCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		for (const char* alias : {"add", "addset", "addcube", "remove", "res", "rename", "equip", "dye", "megaphone"})
			levels[alias] = 3;
		for (const char* alias : {"preview", "nomorph", "noexp", "del", "decompose"})
			levels[alias] = 0;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		std::string itemsXml(ITEM_TEMPLATES_XML);
		itemsXml.replace(itemsXml.rfind("</item_templates>"), std::string_view("</item_templates>").size(), EXTRA_ITEM_TEMPLATES_XML);
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, itemsXml));
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
		dataholders::DataManager::MOTION_DATA.publish(xml::bindString<dataholders::MotionData>(context, "<motion_times/>"));
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA.publish(xml::bindString<dataholders::DecomposableItemsData>(context, DECOMPOSABLE_ITEMS_XML));
		// NameConfig's defaults (NameConfig.java: gameserver.name.pattern "[a-zA-Z]{2,16}"): the test process loads no properties
		configs::main::NameConfig::CHAR_NAME_PATTERN.set(std::wregex(L"[a-zA-Z]{2,16}"));
		configs::main::NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(std::nullopt);
		configs::main::NameConfig::FORBIDDEN_WORDS.set({});
		executor = dynamic_cast<runtime::DeterministicExecutor*>(utils::ThreadPoolManager::installedBackend());
		ASSERT_NE(executor, nullptr);
	}

	void TearDown() override {
		for (Player* player : stored) {
			player->getController().cancelAllTasks();
			world::World::getInstance().removeObject(*player);
		}
		stored.clear();
		loaded.clear();
		CommandTest::TearDown();
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA.resetForTests();
		dataholders::DataManager::ENCHANT_DATA.resetForTests(); // published by the //equip case
		dataholders::DataManager::MOTION_DATA.resetForTests();
		dataholders::DataManager::ITEM_SET_DATA.resetForTests();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel) {
		Player& player = connected(objectId, name, accessLevel);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		player.getCommonData()->setOnline(true);
		stored.push_back(&player);
		return player;
	}

	/** An item loaded into the cube the way the DAO does (onLoadHandler: no packet) */
	Item& stored_(Player& player, int32_t objId, int32_t itemId, int64_t count) {
		runtime::Ref<Item> item = loadedItem(objId, itemId, count, StorageType::CUBE);
		player.getInventory().onLoadHandler(*item);
		loaded.push_back(item);
		return *item;
	}

	/**
	 * An item loaded as equipped in `slot` (Equipment.onLoadHandler, as PlayerService.loadPlayer does); the player knows the sword and the chain
	 * armor skills that Equipment.checkAvailableEquipSkills asks (ItemGroup.java:16, :52)
	 */
	Item& equipped(Player& player, int32_t objId, int32_t itemId, int64_t slot) {
		if (!player.getSkillList()->isSkillPresent(SWORD_SKILL))
			player.setSkillList(model::skill::PlayerSkillList::create({
				model::skill::PlayerSkillEntry::create(SWORD_SKILL, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED),
				model::skill::PlayerSkillEntry::create(42, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
		runtime::Ref<Item> item = loadedItem(objId, itemId, 1, StorageType::CUBE, slot, true);
		player.getEquipment().onLoadHandler(*item);
		loaded.push_back(item);
		return *item;
	}

	std::vector<uint8_t> system(SM_SYSTEM_MESSAGE&& packet, size_t index = 0) { return serialized(packet, client(index).con()); }

	/** how often the client of `index` was sent exactly these bytes */
	size_t count(const std::vector<uint8_t>& bytes, size_t index = 0) {
		const std::vector<std::vector<uint8_t>> sent = client(index)->sentBytes();
		return static_cast<size_t>(std::count(sent.begin(), sent.end(), bytes));
	}

	/** how many packets the client of `index` was sent that hold the text */
	size_t countText(std::string_view text, size_t index = 0) {
		const std::vector<std::vector<uint8_t>> sent = client(index)->sentBytes();
		return static_cast<size_t>(std::count_if(sent.begin(), sent.end(), [text](const std::vector<uint8_t>& p) { return holdsText(p, text); }));
	}

	void clearAll() {
		for (const std::unique_ptr<TestClient>& c : clients)
			(*c)->clearSent();
	}

	xml::LoadContext context;
	runtime::DeterministicExecutor* executor = nullptr;
	std::vector<Player*> stored;
	std::vector<runtime::Ref<Item>> loaded;
};

/** Add.java:35-77: a name nobody has, an unknown item, items and Kinah to self and to another player, the count checks */
TEST_F(ItemCommandsTest, AddGivesItemsAndKinah) {
	Player& gm = online(733000, "Warden", 3);
	Player& bravo = online(733001, "Bravo", 0);
	handlers::admincommands::Add add;

	EXPECT_TRUE(add.process(gm, args({"Nobody"})));
	EXPECT_EQ(count(system(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"))), 1u) << "no item id: the parameter is a player name";
	EXPECT_TRUE(add.process(gm, args({"199999999"})));
	EXPECT_EQ(count(message("Invalid item.")), 1u);
	clearAll();

	EXPECT_TRUE(add.process(gm, args({"162000002", "3"})));
	EXPECT_EQ(gm.getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 3);
	EXPECT_EQ(countText("You gave"), 0u) << "no message to yourself";

	EXPECT_TRUE(add.process(gm, args({"Bravo", "kinah", "500"})));
	EXPECT_EQ(bravo.getInventory().getKinah(), 500);
	EXPECT_EQ(countText("You gave 500 x "), 1u);
	EXPECT_EQ(countText("You received 500 x ", 1), 1u);
	clearAll();

	EXPECT_TRUE(add.process(gm, args({"kinah", "0"})));
	EXPECT_EQ(count(message("Invalid item count.")), 1u) << "kinah <amount> with two parameters";
	EXPECT_TRUE(add.process(gm, args({"Bravo", "kinah", "9223372036854775807"})));
	EXPECT_EQ(count(message("Invalid item count.")), 2u) << "500 + Long.MAX_VALUE wraps below 0";
	EXPECT_EQ(bravo.getInventory().getKinah(), 500);
	EXPECT_TRUE(add.process(gm, args({"162000002", "127000"})));
	EXPECT_EQ(count(message("Invalid item count.")), 3u) << "127000 / 1000 = 127 stacks > 126";
	EXPECT_TRUE(add.process(gm, args({"162000002", "126999"})));
	EXPECT_EQ(count(message("Invalid item count.")), 3u) << "126 stacks pass the check";
}

/** Remove.java:22-83: the player, the item link split by a space, the count checks */
TEST_F(ItemCommandsTest, RemoveTakesItemsFromAPlayer) {
	Player& gm = online(733010, "Warden", 3);
	Player& bravo = online(733011, "Bravo", 0);
	stored_(bravo, 990001, MINOR_LIFE_POTION, 5);
	handlers::admincommands::Remove remove;
	const std::vector<uint8_t> syntax = message("Syntax: //remove <player> <item ID|item @link> [quantity]");

	EXPECT_TRUE(remove.process(gm, args({"Nobody", "162000002"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player isn't online."), syntax}));
	clearAll();
	EXPECT_TRUE(remove.process(gm, args({"Bravo", "162000002", "10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player only has 5 of this item."), syntax}));
	clearAll();
	EXPECT_TRUE(remove.process(gm, args({"Bravo", "182004793"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Player doesn't have that item."), syntax}));
	clearAll();
	EXPECT_TRUE(remove.process(gm, args({"Bravo", "abc"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid item ID."), syntax}));
	clearAll();
	EXPECT_TRUE(remove.process(gm, args({"Bravo", "162000002", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid number parameter passed."), syntax}));
	clearAll();
	EXPECT_TRUE(remove.process(gm, args({"Bravo", "162000002", "0"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid item count."), syntax}));
	clearAll();

	EXPECT_TRUE(remove.process(gm, args({"Bravo", "[item:", "162000002", "2"})));
	EXPECT_EQ(bravo.getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 3) << "[item: and the id joined, the count third";
	EXPECT_EQ(client()->sentBytes(), exactly({message("Successfully removed 2x [item:162000002] from Bravo's inventory.")}));
	EXPECT_EQ(count(message("Admin removed 2x [item:162000002] from your inventory.", 1), 1), 1u);
	EXPECT_TRUE(remove.process(gm, args({"Bravo", "[item:162000002;ver6;;;;]"})));
	EXPECT_EQ(bravo.getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 2) << "a link: the id after [item:, count 1";
}

/** AddSet.java:24-81, AddCube.java:20-48 */
TEST_F(ItemCommandsTest, AddSetAndAddCubeRefusalsAndTheCube) {
	Player& gm = online(733020, "Warden", 3);
	Player& bravo = online(733021, "Bravo", 0);
	handlers::admincommands::AddSet addSet;
	handlers::admincommands::AddCube addCube;

	EXPECT_TRUE(addSet.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("syntax //addset <player> <itemset ID>"), message("syntax //addset <itemset ID>")}));
	clearAll();
	EXPECT_TRUE(addSet.process(gm, args({"5"})));
	EXPECT_TRUE(addSet.process(gm, args({"Nobody", "5"})));
	EXPECT_TRUE(addSet.process(gm, args({"Bravo"})));
	EXPECT_TRUE(addSet.process(gm, args({"Bravo", "x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("ItemSet does not exist with id 5"), message("Could not find a player by that name."),
										  message("Occurs an error."), message("You must give number to itemset ID.")}))
		<< "params[1] of a name alone is the ArrayIndexOutOfBoundsException the second catch takes";
	clearAll();

	EXPECT_TRUE(addCube.process(gm, args({})));
	EXPECT_TRUE(addCube.process(gm, args({"Nobody"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Syntax: //addcube <player name>"), message("The player Nobody is not online.")}));
	clearAll();
	const int32_t limit = configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.exchange(11); // custom.properties:154
	const int32_t before = bravo.getInventory().getLimit();
	EXPECT_TRUE(addCube.process(gm, args({"Bravo"})));
	configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.store(limit);
	EXPECT_EQ(bravo.getInventory().getLimit(), before + 9) << "CubeExpandService.npcExpand";
	EXPECT_EQ(count(message("9 cube slots successfully added to player Bravo!")), 1u);
	EXPECT_EQ(count(message("Admin Warden gave you a cube expansion!", 1), 1), 1u);
}

/** Res.java:20-51: no target, an alive player, the prompt, the instant revive, an unknown parameter */
TEST_F(ItemCommandsTest, ResPromptsOrRevivesTheTarget) {
	Player& gm = online(733030, "Warden", 3);
	Player& bravo = online(733031, "Bravo", 0);
	handlers::admincommands::Res res;

	EXPECT_TRUE(res.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("No target selected.")}));
	clearAll();
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(bravo));
	clearAll();
	EXPECT_TRUE(res.process(gm, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("That player is already alive.")}));
	clearAll();

	bravo.setLifeStats(std::make_unique<DeadPlayerLifeStats>(bravo));
	ASSERT_TRUE(bravo.isDead());
	EXPECT_TRUE(res.process(gm, args({"x"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("[Resurrect] Usage: target player and use //res <instant|prompt>")}));
	EXPECT_TRUE(res.process(gm, args({"pro"})));
	EXPECT_EQ(count(serialized(serverpackets::SM_RESURRECT(gm), client(1).con()), 1), 1u) << "\"prompt\".startsWith(\"pro\")";
	EXPECT_TRUE(bravo.getResStatus());
	EXPECT_TRUE(bravo.isDead());
	clearAll();
	// PlayerReviveService.skillRevive runs into the services this executable has not got (HousingService: HouseData or the houses table)
	network::test::LogCapture capture({"com.aionemu.gameserver.utils.chathandlers.ChatCommand"});
	EXPECT_TRUE(res.process(gm, args({"inst"})));
	EXPECT_EQ(count(message("[Resurrect] Usage: target player and use //res <instant|prompt>")), 0u);
	EXPECT_EQ(capture.count("Exception executing chat command \"//res inst\""), 1) << capture.dump();
	gm.setTarget(nullptr);
}

/** Rename.java:32-50: no target and one parameter is World.getPlayer(null); a name the pattern refuses */
TEST_F(ItemCommandsTest, RenameRefusesBeforeTheDatabase) {
	Player& gm = online(733040, "Warden", 3);
	Player& bravo = online(733041, "Bravo", 0);
	handlers::admincommands::Rename rename;

	EXPECT_THROW(rename.execute(gm, args({"Newname"})), runtime::NullPointerException) << "ConcurrentHashMap.get(null)";
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(bravo));
	clearAll();
	EXPECT_TRUE(rename.process(gm, args({"Al9ha"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_WRONG_INPUT())}));
	clearAll();
	EXPECT_TRUE(rename.process(gm, args({"Bravo", "X"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_MSG_EDIT_CHAR_NAME_ERROR_WRONG_INPUT())})) << "one letter";
	EXPECT_EQ(bravo.getName(), "Bravo");
	gm.setTarget(nullptr);
}

/** Equip.java:42-155: socket, unsocket, enchant and temper on the equipped items of the target */
TEST_F(ItemCommandsTest, EquipSocketsEnchantsAndTempersTheTarget) {
	Player& gm = online(733050, "Warden", 3);
	Player& bravo = online(733051, "Bravo", 0);
	Item& sword = equipped(bravo, 990051, TRAINING_SWORD, model::items::getSlotIdMask(model::items::ItemSlot::MAIN_HAND));
	Item& hauberk = equipped(bravo, 990052, TRAINING_HAUBERK, model::items::getSlotIdMask(model::items::ItemSlot::TORSO));
	handlers::admincommands::Equip equip;

	EXPECT_TRUE(equip.process(gm, args({"socket", "162000002", "5", "Bravo"})));
	EXPECT_TRUE(equip.process(gm, args({"socket", "167000226", "0", "Bravo"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid manastone."), message("Count must be greater than 0.")}));
	clearAll();
	EXPECT_TRUE(equip.process(gm, args({"socket", "167000226", "Bravo"})));
	EXPECT_EQ(sword.getItemStonesSize(), 0) << "params[2] is the limit: Integer.parseInt(\"Bravo\") throws";
	clearAll();

	EXPECT_TRUE(equip.process(gm, args({"socket", "167000226", "5", "Bravo"})));
	EXPECT_EQ(sword.getItemStonesSize(), 1) << "m_slots 1";
	EXPECT_EQ(hauberk.getItemStonesSize(), 1);
	EXPECT_EQ(countText("1x "), 1u) << "the most stones one item took";
	EXPECT_EQ(countText(" added 5x ", 1), 1u) << "the target's message names the count parameter";
	clearAll();
	EXPECT_TRUE(equip.process(gm, args({"socket", "167000226", "5", "Bravo"})));
	EXPECT_EQ(count(message("There are no free slots on any equipped items.")), 1u);
	EXPECT_TRUE(equip.process(gm, args({"unsocket", "Bravo"})));
	EXPECT_EQ(sword.getItemStonesSize(), 0);
	EXPECT_EQ(hauberk.getItemStonesSize(), 0);
	EXPECT_EQ(countText(" removed all manastones from all your equipped items.", 1), 1u);
	clearAll();

	// below +20 EnchantService.setEnchantLevel grants no breakthrough skill; the empty EnchantData has no bonus stats for the two groups
	xml::LoadContext enchantContext;
	dataholders::DataManager::ENCHANT_DATA.publish(xml::bindString<dataholders::EnchantData>(enchantContext, "<enchant_templates/>"));
	EXPECT_TRUE(equip.process(gm, args({"enchant", "15", "Bravo"})));
	EXPECT_EQ(sword.getEnchantLevel(), 15);
	EXPECT_EQ(hauberk.getEnchantLevel(), 15);
	EXPECT_TRUE(sword.isAmplified()) << "15 > max_enchant 10";
	EXPECT_EQ(countText(" enchanted all your equipped items to +15.", 1), 1u);
	EXPECT_TRUE(equip.process(gm, args({"enchant", "10", "Bravo"})));
	EXPECT_FALSE(sword.isAmplified()) << "10 is not above max_enchant 10";
	EXPECT_TRUE(equip.process(gm, args({"enchant", "-4", "Bravo"})));
	EXPECT_EQ(sword.getEnchantLevel(), 0);
	EXPECT_FALSE(sword.isAmplified());
	EXPECT_TRUE(equip.process(gm, args({"temper", "3", "Bravo"})));
	EXPECT_EQ(sword.getTempering(), 0) << "no max_tampering: untouched";
	EXPECT_EQ(countText(" tempered all your equipped items to +3.", 1), 1u);

	clearAll();
	EXPECT_TRUE(equip.process(gm, args({"enchant", "2"})));
	EXPECT_EQ(client()->sentBytes(), info("Enchanted all equipped items to +2."))
		<< "no player parameter and no target: the admin himself";
	EXPECT_TRUE(equip.process(gm, args({"other"})));
	EXPECT_EQ(count(info("Enchants all equipped items.").front()), 0u);
}

/** Dye.java:36-97: nothing visible, nothing dyeable, the dye item, a color name, a hex code, an invalid color, the removal */
TEST_F(ItemCommandsTest, DyeColorsTheVisibleEquipment) {
	Player& gm = online(733060, "Warden", 3);
	Player& bravo = online(733061, "Bravo", 0);
	handlers::admincommands::Dye dye;
	gm.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(bravo));
	clearAll();

	EXPECT_TRUE(dye.process(gm, args({"red"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NO_TARGET_ITEM())}));
	clearAll();
	Item& sword = equipped(bravo, 990061, TRAINING_SWORD, model::items::getSlotIdMask(model::items::ItemSlot::MAIN_HAND));
	EXPECT_TRUE(dye.process(gm, args({"red"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_CHANGE_ERROR_CANNOTDYE(sword.getL10n()))}));
	clearAll();

	Item& hauberk = equipped(bravo, 990062, TRAINING_HAUBERK, model::items::getSlotIdMask(model::items::ItemSlot::TORSO));
	EXPECT_TRUE(dye.process(gm, args({"red"})));
	EXPECT_EQ(hauberk.getItemColor(), utils::JavaColor::RED.getRGB() & 0xFFFFFF) << "java.awt.Color.RED (Item.setItemColor drops the alpha)";
	EXPECT_EQ(sword.getItemColor(), std::nullopt) << "not dyeable";
	EXPECT_EQ(countText(" has changed the color of your visible equipment to: ", 1), 1u);
	EXPECT_TRUE(dye.process(gm, args({"169120000"})));
	EXPECT_EQ(hauberk.getItemColor(), 0xc22626) << "the paint's color";
	EXPECT_TRUE(dye.process(gm, args({"#00ff00"})));
	EXPECT_EQ(hauberk.getItemColor(), 0x00ff00);
	EXPECT_TRUE(dye.process(gm, args({"0X0000Ff"})));
	EXPECT_EQ(hauberk.getItemColor(), 0x0000ff);
	clearAll();
	EXPECT_TRUE(dye.process(gm, args({"zz"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid color.")}));
	EXPECT_EQ(hauberk.getItemColor(), 0x0000ff);
	EXPECT_TRUE(dye.process(gm, args({"0"})));
	EXPECT_EQ(hauberk.getItemColor(), std::nullopt);
	EXPECT_EQ(countText("Removed dyeing from "), 1u);
	gm.setTarget(nullptr);
}

/** Megaphone.java:24-85: the colors of the megaphone items (descending), the faction label prefixes, the refusals */
TEST_F(ItemCommandsTest, MegaphoneBroadcastsInTheColorOfItsItem) {
	Player& gm = online(733070, "Warden", 3);
	handlers::admincommands::Megaphone megaphone; // its colors: the first construction in this process, after SetUp published the rows

	EXPECT_TRUE(megaphone.process(gm, args({"2", "ely", "Herald", "hello", "world"})));
	EXPECT_EQ(count(serialized(serverpackets::SM_MEGAPHONE(serverpackets::SM_MEGAPHONE::FactionLabel::ELYOS, "Herald", "hello world", AZURE_MEGAPHONE),
				  client().con())),
		1u)
		<< "color 2 is 73eeee, after caf264";
	EXPECT_TRUE(megaphone.process(gm, args({"as", "Herald", "hi"})));
	EXPECT_EQ(count(serialized(serverpackets::SM_MEGAPHONE(serverpackets::SM_MEGAPHONE::FactionLabel::ASMODIANS, "Herald", "hi", MINT_MEGAPHONE),
				  client().con())),
		1u)
		<< "no color id: the first color";
	clearAll();
	EXPECT_TRUE(megaphone.process(gm, args({"3", "none", "Herald", "hi"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid color ID.")}));
	EXPECT_TRUE(megaphone.process(gm, args({"x", "Herald", "hi"})));
	EXPECT_EQ(countText("Color IDs: "), 1u) << "an unknown label: the syntax info";
	EXPECT_THROW(megaphone.execute(gm, args({"0", "none", "Herald", "hi"})), runtime::IndexOutOfBoundsException) << "colors.get(-1)";
}

/** Nomorph (NoMorph.java:14-24), NoExp.java:18-23: the toggles and their messages */
TEST_F(ItemCommandsTest, NoMorphAndNoExpToggle) {
	Player& player = online(733080, "Walker", 0);
	handlers::playercommands::Nomorph nomorph;
	handlers::playercommands::NoExp noexp;

	const bool noExp = player.getCommonData()->getNoExp();
	EXPECT_TRUE(noexp.process(player, args({})));
	EXPECT_EQ(player.getCommonData()->getNoExp(), !noExp);
	EXPECT_TRUE(noexp.process(player, args({})));
	EXPECT_EQ(player.getCommonData()->getNoExp(), noExp);
	EXPECT_EQ(client()->sentBytes(),
		exactly({info("Experience rewards are now " + utils::ChatUtil::color("inactive", utils::JavaColor::RED) + ".").front(),
			info("Experience rewards are now " + utils::ChatUtil::color("active", utils::JavaColor::GREEN) + ".").front()}));
	clearAll();

	const int32_t templateId = player.getObjectTemplate()->getTemplateId();
	EXPECT_TRUE(nomorph.process(player, args({})));
	EXPECT_EQ(player.getTransformModel().getEventModelId(), templateId);
	EXPECT_EQ(countText("Transformation appearance is now "), 1u);
	EXPECT_TRUE(nomorph.process(player, args({})));
	EXPECT_EQ(player.getTransformModel().getEventModelId(), 0);
	EXPECT_EQ(count(info("Transformation appearance is now " + utils::ChatUtil::color("active", utils::JavaColor::GREEN) + ".").front()), 1u);
}

/** Del.java:21-50 */
TEST_F(ItemCommandsTest, DelDeletesFromTheInventory) {
	Player& player = online(733090, "Walker", 0);
	stored_(player, 990091, MINOR_LIFE_POTION, 5);
	handlers::playercommands::Del del;

	EXPECT_TRUE(del.process(player, args({"abc"})));
	EXPECT_TRUE(del.process(player, args({"162000002", "0"})));
	EXPECT_TRUE(del.process(player, args({"182004793"})));
	EXPECT_TRUE(del.process(player, args({"162000002", "9"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("Invalid item."), message("Invalid item count."), message("You don't have that item."),
										  message("You only have 5.")}));
	clearAll();
	EXPECT_TRUE(del.process(player, args({"162000002", "2"})));
	EXPECT_EQ(player.getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 3);
	EXPECT_EQ(countText("Deleted 2x "), 1u);
	EXPECT_TRUE(del.process(player, args({"162000002"})));
	EXPECT_EQ(player.getInventory().getItemCountByItemId(MINOR_LIFE_POTION), 2) << "count 1 by default";
}

/** Decompose.java:30-97: the refusals, then two of three Juicy Pepentos opened one per casting delay */
TEST_F(ItemCommandsTest, DecomposeOpensItemsUntilTheCount) {
	Player& player = online(733100, "Walker", 0);
	stored_(player, 990101, MINOR_LIFE_POTION, 1);
	stored_(player, 990102, JUICY_PEPENTO, 3);
	handlers::playercommands::Decompose decompose;

	EXPECT_TRUE(decompose.process(player, args({"182004793"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_NO_TARGET_ITEM())}));
	clearAll();
	EXPECT_TRUE(decompose.process(player, args({"162000002"})));
	EXPECT_EQ(count(system(SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_IT_CAN_NOT_BE_DECOMPOSED(
		player.getInventory().getFirstItemByItemId(MINOR_LIFE_POTION)->getItemTemplate()->getL10n()))),
		1u);
	EXPECT_FALSE(player.getController().hasTask(model::TaskId::SKILL_USE));

	EXPECT_TRUE(decompose.process(player, args({"152000065", "2"})));
	EXPECT_TRUE(player.getController().hasTask(model::TaskId::SKILL_USE));
	executor->advance(std::chrono::milliseconds(10 + 3100 * 3 + 3000));
	EXPECT_EQ(player.getInventory().getItemCountByItemId(JUICY_PEPENTO), 1);
	EXPECT_EQ(player.getInventory().getItemCountByItemId(PEPENTO), 4) << "two of them, 2 Pepento each";
	EXPECT_EQ(countText("Decomposing finished: Processed 2x "), 1u);
	EXPECT_FALSE(player.getController().hasTask(model::TaskId::SKILL_USE));
}

/** Decompose.java:60-72: a move aborts the task (the observer's abort; the item in work is not counted) */
TEST_F(ItemCommandsTest, DecomposeIsAbortedByAMove) {
	Player& player = online(733110, "Walker", 0);
	stored_(player, 990111, JUICY_PEPENTO, 3);
	handlers::playercommands::Decompose decompose;

	EXPECT_TRUE(decompose.process(player, args({"152000065"})));
	executor->advance(std::chrono::milliseconds(10 + 3100 + 3000));
	player.getObserveController()->notifyMoveObservers();
	EXPECT_EQ(countText("Decomposing aborted: Processed 1x "), 1u) << "two started, max(0, 2 - 1)";
	EXPECT_FALSE(player.getController().hasTask(model::TaskId::SKILL_USE));
	const int64_t left = player.getInventory().getItemCountByItemId(JUICY_PEPENTO);
	executor->advance(std::chrono::milliseconds(20000));
	EXPECT_EQ(player.getInventory().getItemCountByItemId(JUICY_PEPENTO), left) << "nothing more after the abort";
}

/** Preview.java:47-74: an emotion card plays its emotion */
TEST_F(ItemCommandsTest, PreviewPlaysAnEmotionCard) {
	Player& player = online(733120, "Walker", 0);
	handlers::playercommands::Preview preview;

	EXPECT_TRUE(preview.process(player, args({"169600001"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(serverpackets::SM_EMOTION(player, model::EmotionType::EMOTE_END), client().con()),
										  serialized(serverpackets::SM_EMOTION(player, model::EmotionType::EMOTE, 64, 0), client().con())}));
	EXPECT_EQ(executor->pendingTaskCount(), 0u) << "no preview reset";
}

/** Preview.java:76-176: the refusals, two items of one parameter in a color, the reset after 10 seconds */
TEST_F(ItemCommandsTest, PreviewShowsEquipmentForTenSeconds) {
	Player& player = online(733130, "Walker", 0);
	handlers::playercommands::Preview preview;

	EXPECT_TRUE(preview.process(player, args({"nothing"})));
	EXPECT_TRUE(preview.process(player, args({"162000002"})));
	EXPECT_EQ(client()->sentBytes(), exactly({system(SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NO_TARGET_ITEM()),
										  system(SM_SYSTEM_MESSAGE::STR_MSG_CHANGE_ITEM_SKIN_PREVIEW_INVALID_COSMETIC())}));
	EXPECT_THROW(preview.execute(player, args({",,"})), runtime::NoSuchElementException) << "\",,\".split(...) is empty: List.getFirst()";
	clearAll();

	EXPECT_TRUE(preview.process(player, args({"[item:110500003][item:100000094],red"})));
	const std::vector<std::vector<uint8_t>> shown = client()->sentBytes();
	ASSERT_EQ(shown.size(), 3u) << "SM_CUSTOM_SETTINGS, SM_UPDATE_PLAYER_APPEARANCE, the message";
	EXPECT_EQ(javaOpcodeOf(shown[1]), SM_UPDATE_PLAYER_APPEARANCE_OPCODE);
	EXPECT_TRUE(holdsText(shown[2], "Previewing the following items for 10 seconds (color: "));
	EXPECT_TRUE(holdsText(shown[2], "\n\t" + utils::ChatUtil::item(TRAINING_HAUBERK))) << "split between the two item tags";
	EXPECT_TRUE(holdsText(shown[2], "\n\t" + utils::ChatUtil::item(TRAINING_SWORD)));
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "the preview reset";
	clearAll();
	executor->advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(message("Preview time ended.")), 0u);
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(message("Preview time ended.")), 1u);
	EXPECT_EQ(executor->pendingTaskCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
