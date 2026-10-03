#pragma once

// Shared fixture of the M5c stage-2 craft client packet tests (m5c-plan.md C-04, C-06): P5-15's tests/cm_ak/CraftPacketTest.cpp (CM_CRAFT) and
// P5-16's tests/cm_lz/RecipePacketsTest.cpp (CM_RECIPE_DELETE, and CM_USE_ITEM on a recipe item, whose CraftLearnAction is P5-07's C-05), which
// includes this header by relative path as it includes EconomyPacketTestSupport.h. Each runImpl hands its fields to the craft lane's bodies
// (CraftService.startCrafting, RecipeList.deleteRecipe, RecipeService through CraftLearnAction); the cases observe what those do with them.
//
// - CraftPacketTest is ItemPacketTest (the spawned level-1 ELYOS WARRIOR "Holder", 710101, at (100, 100, 50) in a Poeta map instance, a real
//   AionConnection whose send queue the cases read) with the craft rows below published: ITEM_DATA holds ItemPacketTestSupport.h's rows and
//   CRAFT_PACKET_ITEM_ROWS, SKILL_DATA its skill row and CRAFT_PACKET_SKILL_ROWS, RECIPE_DATA CRAFT_PACKET_RECIPES_XML and SKILL_TREE_DATA none.
//   Every row is copied verbatim from the shipped data (game-server/data/static_data, file:line beside each).
// - The player knows Cooking at level 1 and no recipe, set the way PlayerSkillListDAO and PlayerRecipesDAO load them (no packet); a case adds
//   what it needs the same way.
// - A crafting station is a StaticObject whose template is the station's item_templates.xml row (StaticObjectSpawnManager.java:23, 31:
//   ITEM_DATA.getItemTemplate), spawned at an offset from the player in his map instance and put into his known list.
// - A craft a case starts is aborted in TearDown (the CraftingTask holds its requester). Its first tick is 1 s away on the fixture's
//   DeterministicExecutor, which the cases never advance, so a started craft has sent only its start packets.
// - The distances are `oracle.py m5c-craft --no-profile --set gameserver.event.service.disabled_events=* --recipe 155001381`'s craft.station:
//   CM_CRAFT's packet range is 10 centre to centre, checkCraft's effective range 5.25 (5 + the player's bound radius 0.25 + the station's 0),
//   both compared strictly (dx*dx + dy*dy + dz*dz < range*range).
// - RecipeList.addRecipe and deleteRecipe write through PlayerRecipesDAO. A case whose Java flow needs the write to succeed calls
//   CRAFT_PACKET_REQUIRE_DATABASE(): the economy test database of tests/economy/P5-09a/EconomyTestSupport.h (aion_gs_test_economy, recreated
//   once per process under its named lock; the case is skipped without AION_TEST_GS_DATABASE_URL), MailPacketTestSupport.h's pattern: the case
//   replaces the holder's players row (player_recipes cascades), and TearDown shuts DatabaseFactory down again, so a case that follows in one
//   process has no database, as it has none under ctest (one process per case). Without a database DB.insertUpdate catches the SQLException and
//   returns false, so the recipe list keeps what it had; the cases that observe that assert first that no database is open.
// - Like EconomyPacketTestSupport.h, it must not be mixed with tests/world/WorldTestSupport.h in one executable run (m5c-plan.md §18.7).

#include "EconomyPacketTestSupport.h"
#include "../economy/P5-09a/EconomyTestSupport.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_UPDATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/skillengine/task/CraftingTask.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {

namespace craftdb = ::aion::gameserver::economy::test;

// the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:65, :117, :169; State.IN_GAME)
inline constexpr int32_t CM_USE_ITEM_OPCODE = 37;
inline constexpr int32_t CM_RECIPE_DELETE_OPCODE = 89;
inline constexpr int32_t CM_CRAFT_OPCODE = 141;

// ServerPacketsOpcodes.java:198-199, :259-260
inline constexpr int32_t SM_CRAFT_ANIMATION_OPCODE = 180;
inline constexpr int32_t SM_CRAFT_UPDATE_OPCODE = 181;
inline constexpr int32_t SM_LEARN_RECIPE_OPCODE = 241;
inline constexpr int32_t SM_RECIPE_DELETE_OPCODE = 242;

inline constexpr int32_t CRAFT_HOLDER_ID = 710101; // ItemPacketTest's player
inline constexpr int32_t CRAFT_HOLDER_ACCOUNT = 9901;

// skills (skill_templates.xml; Profession.java:11-21)
inline constexpr int32_t COOKING = 40001;
inline constexpr int32_t ARMORSMITHING = 40003;
inline constexpr int32_t MORPH = 40009;

// items (CRAFT_PACKET_ITEM_ROWS)
inline constexpr int32_t OVEN = 150000009;
inline constexpr int32_t AETHER_POWDER = 152000901;
inline constexpr int32_t ININA = 152001001;
inline constexpr int32_t RECIPE_ROAST_ININA = 152201381;  // <craftlearn recipeid="155001381"/>
inline constexpr int32_t RECIPE_ROAST_CONIDE = 152206386; // <craftlearn recipeid="155006386"/>, ASMODIANS
inline constexpr int32_t MORPH_METHOD_ININA = 152200002;  // <craftlearn recipeid="155000002"/>
inline constexpr int32_t ROAST_ININA = 160001001;
inline constexpr int32_t TASTY_ROAST_ININA = 160001051;
inline constexpr int32_t SALT = 169400096;
inline constexpr int32_t COOKING_STONE = 169401081; // CraftService.getBonusReqItem(40001) (CraftService.java:242-243)

// recipes (CRAFT_PACKET_RECIPES_XML)
inline constexpr int32_t ROAST_ININA_RECIPE = 155001381;  // cooking 1, ELYOS: 1 Inina + 2 Salt -> 2 Roast Inina; the gate's C19 recipe
inline constexpr int32_t ININA_MORPH_RECIPE = 155000002;  // morph 1, ELYOS, dp 200: 1 Aether Powder -> 3 Inina
inline constexpr int32_t ROAST_CONIDE_RECIPE = 155006386; // cooking 1, ASMODIANS
inline constexpr int32_t METAL_PLATE_RECIPE = 155004042;  // armorsmithing 1, ELYOS; its row is not in CRAFT_PACKET_RECIPES_XML (see there)

/** item_templates.xml, verbatim: the rows of the craft cases that ItemPacketTestSupport.h lacks */
inline constexpr std::string_view CRAFT_PACKET_ITEM_ROWS = R"xml(
	<!-- :743547 -->
	<item_template id="150000009" name="Oven" level="1" cName="cooking" mask="4190" quality="COMMON" price="100" restrict="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="700104"/>
	<!-- :744285 -->
	<item_template id="152000901" name="Aether Powder" level="10" cName="od_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="5" desc="702030"/>
	<!-- :744358 -->
	<item_template id="152001001" name="Inina" level="10" cName="shell_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="5" race="ELYOS" desc="702027"/>
	<!-- :745549 -->
	<item_template id="152200002" name="Morph Method: Inina" level="10" cName="rec_l_cv_shell_n_c_10a" mask="12364" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="100" race="ELYOS" desc="731758" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155000002"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :754001 -->
	<item_template id="152201381" name="Recipe: Roast Inina" level="20" cName="rec_l_co_food_phyattack_20a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="550" race="ELYOS" desc="733135" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155001381"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :779865 -->
	<item_template id="152206386" name="Recipe: Roast Conide" level="20" cName="rec_d_co_food_d_phyattack_20a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="550" race="ASMODIANS" desc="737582" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155006386"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :821940 -->
	<item_template id="160001001" name="Roast Inina" level="10" cName="food_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729414" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10054"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :822180 -->
	<item_template id="160001051" name="Tasty Roast Inina" level="10" cName="food_r_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729464" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10086"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :850125 -->
	<item_template id="169400096" name="Salt" level="10" cName="shopmaterial_co_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="50" desc="703774"/>
	<!-- :850246 -->
	<item_template id="169401081" name="Cooking Enhancement Stone" level="1" cName="co_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789759"/>
)xml";

/** skill_templates.xml, verbatim: Cooking (:204340) and Morph Substances (:204380) */
inline constexpr std::string_view CRAFT_PACKET_SKILL_ROWS = R"xml(
	<!-- :204340 -->
	<skill_template skill_id="40001" name="Cooking" nameId="280383" stack="COOKING" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204380 -->
	<skill_template skill_id="40009" name="Morph Substances" nameId="282929" stack="CONVERT" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
)xml";

/**
 * recipe/recipe_templates.xml, verbatim rows. 155004042 (Design: Metal Plate's recipe, :34095) is left out on purpose: a recipe item whose
 * recipe RECIPE_DATA does not hold is how the cases reach RecipeService's "no such recipe" arm, which no shipped recipe item reaches.
 */
inline constexpr std::string_view CRAFT_PACKET_RECIPES_XML = R"xml(<recipe_templates>
	<!-- :8 -->
	<recipe_template id="155000002" nameid="730279" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="152001001" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :12124 -->
	<recipe_template id="155001381" nameid="731656" skillid="40001" race="ELYOS" skillpoint="1" autolearn="1" productid="160001001" quantity="2">
		<components_data>
			<component quantity="1" itemid="152001001"/>
			<component quantity="2" itemid="169400096"/>
		</components_data>
		<comboproduct itemid="160001051"/>
	</recipe_template>
	<!-- :48134 -->
	<recipe_template id="155006386" nameid="736098" skillid="40001" race="ASMODIANS" skillpoint="1" autolearn="1" productid="160002001" quantity="2">
		<components_data>
			<component quantity="1" itemid="152001051"/>
			<component quantity="2" itemid="169400096"/>
		</components_data>
		<comboproduct itemid="160002051"/>
	</recipe_template>
</recipe_templates>)xml";

/** ItemPacketTestSupport.h's item rows with CRAFT_PACKET_ITEM_ROWS appended inside the same <item_templates> */
inline std::string craftPacketItemTemplatesXml() {
	std::string xml(ITEM_TEMPLATES_XML);
	xml.insert(xml.rfind("</item_templates>"), CRAFT_PACKET_ITEM_ROWS);
	return xml;
}

/** ItemPacketTestSupport.h's skill row with CRAFT_PACKET_SKILL_ROWS appended inside the same <skill_data> */
inline std::string craftPacketSkillTemplatesXml() {
	std::string xml(SKILL_TEMPLATES_XML);
	xml.insert(xml.rfind("</skill_data>"), CRAFT_PACKET_SKILL_ROWS);
	return xml;
}

class CraftPacketSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	CraftPacketSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** The materials map of CM_CRAFT (item id -> count), as readImpl builds it */
using CraftMaterials = std::unordered_map<int32_t, int64_t>;

/**
 * CM_CRAFT's body as Java's readImpl reads it (CM_CRAFT.java:32-41): C unk, D targetTemplateId, D recipeId, D targetObjId, H materialsCount,
 * C craftType, then per material D itemId and Q count
 */
inline std::vector<uint8_t> craftBody(int32_t unk, int32_t targetTemplateId, int32_t recipeId, int32_t targetObjId,
	std::initializer_list<std::pair<int32_t, int64_t>> materials, int32_t craftType) {
	PacketWriter body;
	body.C(unk).D(targetTemplateId).D(recipeId).D(targetObjId).H(static_cast<int32_t>(materials.size())).C(craftType);
	for (const auto& [itemId, count] : materials)
		body.D(itemId).Q(count);
	return body.data;
}

/** SM_CRAFT_ANIMATION(playerObjId, targetObjectId, skillId, action) as Java writes it (SM_CRAFT_ANIMATION.java writeImpl: D, D, H, C) */
inline std::vector<uint8_t> craftAnimation(int32_t playerObjId, int32_t targetObjectId, int32_t skillId, int32_t action) {
	return javaPacket(SM_CRAFT_ANIMATION_OPCODE, PacketWriter().D(playerObjId).D(targetObjectId).H(skillId).C(action));
}

/**
 * SM_CRAFT_UPDATE of a cancelled craft (action 4) as Java writes it (SM_CRAFT_UPDATE.java:21-33, writeImpl: H skillId, C action, D itemId,
 * D success, D failure, D executionSpeed, D delay - 1000 for the morph skill -, then for action 4 D(1330051) and writeS(null), i.e. H(0))
 */
inline std::vector<uint8_t> craftCancelled(int32_t skillId, int32_t productId) {
	return javaPacket(SM_CRAFT_UPDATE_OPCODE,
		PacketWriter().H(skillId).C(4).D(productId).D(0).D(0).D(0).D(skillId == MORPH ? 1000 : 0).D(1330051).H(0));
}

/** SM_RECIPE_DELETE(recipeId) (SM_RECIPE_DELETE.java writeImpl: D recipeId) */
inline std::vector<uint8_t> recipeDeleted(int32_t recipeId) {
	return javaPacket(SM_RECIPE_DELETE_OPCODE, PacketWriter().D(recipeId));
}

/** SM_LEARN_RECIPE(recipeId) (SM_LEARN_RECIPE.java writeImpl: D recipeId, C 0) */
inline std::vector<uint8_t> recipeLearned(int32_t recipeId) {
	return javaPacket(SM_LEARN_RECIPE_OPCODE, PacketWriter().D(recipeId).C(0));
}

class CraftPacketTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished with the craft rows
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, craftPacketItemTemplatesXml()));
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base row, republished with the craft skills (the base TearDown resets it)
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, craftPacketSkillTemplatesXml()));
		dataholders::DataManager::RECIPE_DATA.publish(xml::bindString<dataholders::RecipeData>(context, CRAFT_PACKET_RECIPES_XML));
		// PlayerSkillList.addSkill asks the skill tree for a new skill; no case adds one, but the holder must exist
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, "<skill_tree/>"));
		setRecipes({});
		setSkills({{COOKING, 1}});
		clearSent();
	}

	void TearDown() override {
		if (f.player) {
			if (runtime::Ptr<skillengine::task::AbstractInteractionTask> task = player().getInteractionTask())
				task->abort(); // the started craft holds its requester
		}
		clearSent();
		given.clear();
		stations.clear(); // before the map instance their positions name
		spawnGroups.clear();
		ItemPacketTest::TearDown();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::RECIPE_DATA.resetForTests();
		if (databaseOpened)
			commons::database::DatabaseFactory::shutdown();
	}

	/** Reads and runs `body` on the player's connection, as AionConnection::processData and the PacketProcessor do */
	template <class P>
	void readAndRun(int32_t opcode, const std::vector<uint8_t>& body) {
		EconomyDriver<P> packet(opcode);
		ASSERT_TRUE(packet.readOn(body, std::shared_ptr<AionConnection>(client->get())));
		packet.runNow();
	}

	/** The player's recipes, loaded like PlayerRecipesDAO.load (no packet) */
	void setRecipes(std::initializer_list<int32_t> recipeIds) {
		player().setRecipeList(model::gameobjects::player::RecipeList::create(std::unordered_set<int32_t>(recipeIds)));
	}

	bool knowsRecipe(int32_t recipeId) { return player().getRecipeList()->isRecipePresent(recipeId); }

	/** The player's skills: the fixture's sword skill and `skills` (id, level), loaded like PlayerSkillListDAO does (no packet) */
	void setSkills(std::initializer_list<std::pair<int32_t, int32_t>> skills) {
		constexpr auto loaded = model::gameobjects::Persistable_PersistentState::UPDATED;
		std::vector<runtime::Ref<model::skill::PlayerSkillEntry>> owned{model::skill::PlayerSkillEntry::create(SWORD_SKILL, 1, 0, loaded)};
		for (const auto& [skillId, level] : skills)
			owned.push_back(model::skill::PlayerSkillEntry::create(skillId, level, 0, loaded));
		std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>> entries(owned.begin(), owned.end());
		player().setSkillList(model::skill::PlayerSkillList::create(entries));
	}

	/** An item loaded into the cube the way the DAO does (onLoadHandler: no packet) */
	Item& give(int32_t objId, int32_t itemId, int64_t count) {
		runtime::Ref<Item> item = loadedItem(objId, itemId, count, StorageType::CUBE);
		player().getInventory().onLoadHandler(*item);
		given.push_back(item);
		return *item;
	}

	int64_t countOf(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }

	const model::templates::item::ItemTemplate* itemTemplate(int32_t itemId) {
		const model::templates::item::ItemTemplate* row = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (row == nullptr)
			throw runtime::NullPointerException("no item row " + std::to_string(itemId));
		return row;
	}

	/**
	 * A crafting station: a StaticObject of the station's item template (StaticObjectSpawnManager.java:23, 31) spawned `dx` metres east of the
	 * player and `dz` above him, known to him (the see notification's packets are cleared)
	 */
	model::gameobjects::StaticObject& station(float dx, float dz = 0.0f, int32_t templateId = OVEN) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, templateId, 0, nullptr);
		const float x = player().getX() + dx;
		const float z = player().getZ() + dz;
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<CraftPacketSpawnTemplate>(*group, x, 100.0f, z));
		runtime::Ref<model::gameobjects::StaticObject> created = model::gameobjects::VisibleObject::create<model::gameobjects::StaticObject>(
			std::make_unique<controllers::StaticObjectController>(), spawn, itemTemplate(templateId));
		created->setPosition(world::WorldPosition::create(210010000, x, 100.0f, z, int8_t{0}, mapInstance->getRegion(x, 100.0f, z)));
		created->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		stations.push_back(created);
		f.knownList().addForTest(*created);
		clearSent();
		return *created;
	}

	/** The craft packets (SM_CRAFT_UPDATE and SM_CRAFT_ANIMATION) of the capture, in order */
	std::vector<std::vector<uint8_t>> craftPackets() {
		std::vector<std::vector<uint8_t>> packets;
		for (const std::vector<uint8_t>& packet : sent()) {
			const int32_t opcode = javaOpcodeOf(packet);
			if (opcode == SM_CRAFT_UPDATE_OPCODE || opcode == SM_CRAFT_ANIMATION_OPCODE)
				packets.push_back(packet);
		}
		return packets;
	}

	/**
	 * The start packets of a craft of `recipe`'s product at `targetObjId` (CraftingTask.onInteractionStart, CraftingTask.java:107-118; the
	 * oracle's packets.start without the DP packets): SM_CRAFT_UPDATE(skill, product, 1000, 1000, 0) and (skill, product, 0, 0, 1), compared
	 * against the server's own serialization (the product's l10n; the bytes are pinned by the sm tests), then SM_CRAFT_ANIMATION(player, target,
	 * skill, 0) and (…, 1) as Java writes them
	 */
	std::vector<std::vector<uint8_t>> craftStart(int32_t skillId, int32_t productId, int32_t targetObjId) {
		using serverpackets::SM_CRAFT_UPDATE;
		return {serializedFor(SM_CRAFT_UPDATE(skillId, itemTemplate(productId), 1000, 1000, 0, 0, 0)),
			serializedFor(SM_CRAFT_UPDATE(skillId, itemTemplate(productId), 0, 0, 1, 0, 0)),
			craftAnimation(player().getObjectId(), targetObjId, skillId, 0), craftAnimation(player().getObjectId(), targetObjId, skillId, 1)};
	}

	/** CraftService.sendCancelCraft's pair (CraftService.java:235-238): SM_CRAFT_UPDATE action 4, SM_CRAFT_ANIMATION(player, target, 0, 2) */
	std::vector<std::vector<uint8_t>> craftRefused(int32_t skillId, int32_t productId, int32_t targetObjId) {
		return {craftCancelled(skillId, productId), craftAnimation(player().getObjectId(), targetObjId, 0, 2)};
	}

	/** @return the CraftingTask the player is interacting with, if one is in progress */
	runtime::Ptr<skillengine::task::CraftingTask> craftInProgress() {
		runtime::Ptr<skillengine::task::CraftingTask> task = runtime::as<skillengine::task::CraftingTask>(player().getInteractionTask());
		return task && task->isInProgress() ? task : nullptr;
	}

	/** @return false (and the case skips) without the test database; see the header comment */
	bool requireDatabase() {
		if (!craftdb::isDatabaseEnabled())
			return false;
		craftdb::setUpDatabaseOnce();
		if (!commons::database::DatabaseFactory::isInitialized()) // shut down by an earlier case of this process
			commons::database::DatabaseFactory::init(craftdb::urlWithDatabase(craftdb::TEST_DATABASE), craftdb::user(), craftdb::password(), 10, 5000);
		databaseOpened = true;
		craftdb::execute("DELETE FROM players WHERE id = " + std::to_string(CRAFT_HOLDER_ID)); // player_recipes cascades
		craftdb::insertPlayer(CRAFT_HOLDER_ID, "Holder", CRAFT_HOLDER_ACCOUNT);
		return true;
	}

	/** The recipe ids of the holder's player_recipes rows, ascending */
	std::vector<int32_t> storedRecipes() {
		std::vector<int32_t> ids;
		auto con = commons::database::DatabaseFactory::getConnection();
		auto rs = con->prepareStatement("SELECT recipe_id FROM player_recipes WHERE player_id = " + std::to_string(CRAFT_HOLDER_ID) +
										" ORDER BY recipe_id")
					  ->executeQuery();
		while (rs->next())
			ids.push_back(rs->getInt(1));
		return ids;
	}

	bool databaseOpened = false;
	std::vector<runtime::Ref<Item>> given;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::StaticObject>> stations;
};

/** Skips the case without the test database (the start of a TEST_F body of CraftPacketTest) */
#define CRAFT_PACKET_REQUIRE_DATABASE()                                                                                                              \
	if (!requireDatabase())                                                                                                                          \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

} // namespace aion::gameserver::network::aion::clientpackets::testing::items
