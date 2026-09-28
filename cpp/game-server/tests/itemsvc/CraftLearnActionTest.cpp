// M5c C-05 (m5c-plan.md §5, §20.3; P5-07): CraftLearnAction, the action of a recipe item (`<craftlearn recipeid="..."/>`), against
// CraftLearnAction.java:26-40: canAct is RecipeService.validateNewRecipe of the action's own recipe id (RecipeService.java:15-50), and act uses
// the item up, says so, and learns the recipe without validating it again (RecipeService.addRecipe(player, recipeid, false),
// RecipeService.java:52-67), answering a learnt recipe with the three-argument SM_ITEM_USAGE_ANIMATION. Tests of C-06.
//
// The rows are verbatim (item_templates.xml, recipe/recipe_templates.xml, skills/skill_templates.xml; the line beside each): Recipe: Roast Inina
// (recipe 155001381, cooking 1, ELYOS), Recipe: Roast Conide (155006386, cooking 1, ASMODIANS), Morph Method: Inina (155000002, morph 1, ELYOS)
// and Design: Metal Plate (155004042, whose recipe row RECIPE_DATA leaves out: the "no such recipe" arm, which no shipped recipe item reaches).
// The player is ItemServicesTest's "Looter" (700101, ELYOS WARRIOR level 1) with Cooking at level 1. RecipeList.addRecipe writes through
// PlayerRecipesDAO: the cases that learn a recipe use the test database of the DAO tests (tests/dao/DaoTestDatabase.h, as EnchantServiceTest
// does) and are skipped without AION_TEST_GS_DATABASE_URL; the other cases never reach the DAO. Packets whose fields the action chooses are
// Java's bytes (SM_ITEM_USAGE_ANIMATION.java:22-30, 73-86; SM_LEARN_RECIPE.java:18-21); the messages and the item update are compared against
// the server's own serialization of the packet Java constructs there (their bytes are pinned by the sm tests).

#include "ItemServicesTestSupport.h"
#include "../dao/DaoTestDatabase.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using model::templates::item::actions::CraftLearnAction;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t SM_LEARN_RECIPE_OPCODE = 241; // ServerPacketsOpcodes.java:259

constexpr int32_t LOOTER_ID = 700101; // ItemServicesTest's player
constexpr int32_t LOOTER_ACCOUNT = 9801;

constexpr int32_t COOKING = 40001;
constexpr int32_t MORPH = 40009;

constexpr int32_t RECIPE_ROAST_ININA = 152201381;
constexpr int32_t RECIPE_ROAST_CONIDE = 152206386;
constexpr int32_t MORPH_METHOD_ININA = 152200002;
constexpr int32_t DESIGN_METAL_PLATE = 152204042;

constexpr int32_t ROAST_ININA_RECIPE = 155001381;
constexpr int32_t ROAST_CONIDE_RECIPE = 155006386;

constexpr int32_t RECIPE_ITEM = 840001; // the recipe item's object id

/** item_templates.xml, verbatim: the recipe items */
constexpr std::string_view RECIPE_ITEM_ROWS = R"xml(
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
	<!-- :770134 -->
	<item_template id="152204042" name="Design: Metal Plate" level="1" cName="item_r_as_q5100" mask="12353" max_stack_count="100" quality="COMMON" price="1" race="ELYOS" desc="704492">
		<actions>
			<craftlearn recipeid="155004042"/>
		</actions>
	</item_template>
	<!-- :779865 -->
	<item_template id="152206386" name="Recipe: Roast Conide" level="20" cName="rec_d_co_food_d_phyattack_20a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="550" race="ASMODIANS" desc="737582" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155006386"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
)xml";

/** skill_templates.xml, verbatim: Cooking (:204340) and Morph Substances (:204380) */
constexpr std::string_view CRAFT_SKILL_ROWS = R"xml(
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

/** recipe/recipe_templates.xml, verbatim rows (155004042, :34095, is left out on purpose: see the file comment) */
constexpr std::string_view RECIPE_TEMPLATES_XML = R"xml(<recipe_templates>
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

/** `base` (an <item_templates> or <skill_data> document) with `rows` appended before its closing tag */
std::string withRows(std::string_view base, std::string_view closingTag, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closingTag), rows);
	return xml;
}

class CraftLearnActionTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished with the recipe items
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", RECIPE_ITEM_ROWS)));
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base rows, republished with the craft skills (the base TearDown resets it)
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(context, withRows(SKILL_TEMPLATES_XML, "</skill_data>", CRAFT_SKILL_ROWS)));
		dataholders::DataManager::RECIPE_DATA.publish(xml::bindString<dataholders::RecipeData>(context, RECIPE_TEMPLATES_XML));
		setRecipes({});
		setSkills({{COOKING, 1}});
	}

	void TearDown() override {
		ItemServicesTest::TearDown();
		dataholders::DataManager::RECIPE_DATA.resetForTests();
	}

	/** The player's recipes, loaded like PlayerRecipesDAO.load (no packet) */
	void setRecipes(std::initializer_list<int32_t> recipeIds) {
		player().setRecipeList(model::gameobjects::player::RecipeList::create(std::unordered_set<int32_t>(recipeIds)));
	}

	bool knowsRecipe(int32_t recipeId) { return player().getRecipeList()->isRecipePresent(recipeId); }

	/** The player's skills (id, level), loaded like PlayerSkillListDAO does (no packet) */
	void setSkills(std::initializer_list<std::pair<int32_t, int32_t>> skills) {
		std::vector<Ref<model::skill::PlayerSkillEntry>> owned;
		for (const auto& [skillId, level] : skills)
			owned.push_back(model::skill::PlayerSkillEntry::create(skillId, level, 0, model::gameobjects::Persistable_PersistentState::UPDATED));
		std::vector<Ptr<model::skill::PlayerSkillEntry>> entries(owned.begin(), owned.end());
		player().setSkillList(model::skill::PlayerSkillList::create(entries));
	}

	/** The item's only action, the CraftLearnAction its <craftlearn> binds */
	const CraftLearnAction& actionOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate == nullptr || itemTemplate->getActions() == nullptr)
			throw runtime::NullPointerException("no actions of item " + std::to_string(itemId));
		const auto& actions = itemTemplate->getActions()->getItemActions();
		EXPECT_EQ(actions.size(), 1u) << itemId;
		const auto* action = dynamic_cast<const CraftLearnAction*>(actions.front().get());
		if (action == nullptr)
			throw runtime::NullPointerException("item " + std::to_string(itemId) + " has no CraftLearnAction");
		return *action;
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }

	/** SM_LEARN_RECIPE(recipeId) as Java writes it (SM_LEARN_RECIPE.java:18-21: D recipeId, C 0) */
	static std::vector<uint8_t> recipeLearned(int32_t recipeId) { return javaPacket(SM_LEARN_RECIPE_OPCODE, PacketWriter().D(recipeId).C(0)); }

	/** @return false (and the case skips) without the test database; the holder's players row is written (player_recipes' foreign key) */
	bool requireDatabase() {
		if (!dao::test::isEnabled())
			return false;
		dao::test::setUpDatabaseOnce();
		dao::test::clearTables();
		dao::test::insertPlayer(LOOTER_ID, "Looter", LOOTER_ACCOUNT);
		return true;
	}

	/** The recipe ids of the player's player_recipes rows, ascending */
	static std::vector<int32_t> storedRecipes() {
		std::vector<int32_t> ids;
		auto con = commons::database::DatabaseFactory::getConnection();
		auto rs = con->prepareStatement("SELECT recipe_id FROM player_recipes WHERE player_id = " + std::to_string(LOOTER_ID) + " ORDER BY recipe_id")
					  ->executeQuery();
		while (rs->next())
			ids.push_back(rs->getInt(1));
		return ids;
	}
};

#define CRAFT_LEARN_REQUIRE_DATABASE()                                                                                                              \
	if (!requireDatabase())                                                                                                                         \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

// ---------------------------------------------------------------------------------------------------------------------------------------- canAct

// CraftLearnAction.java:38-40: a recipe the player may learn (validateNewRecipe returns its template) - no message; the items are not read
TEST_F(CraftLearnActionTest, CanActPassesALearnableRecipeSilently) {
	Item& recipe = stored(RECIPE_ITEM, RECIPE_ROAST_ININA, 1);

	EXPECT_TRUE(actionOf(RECIPE_ROAST_ININA).canAct(player(), Ptr<Item>(recipe), nullptr));
	EXPECT_TRUE(actionOf(RECIPE_ROAST_ININA).canAct(player(), nullptr, nullptr)) << "canAct reads neither item";
	EXPECT_TRUE(sent().empty());
}

// RecipeService.java:21-47 for the action's own recipe id: a known recipe, a recipe of the other race (the Asmodian Roast Conide for the Elyos
// player), a recipe of a skill the player lacks (the morph) and a recipe RECIPE_DATA does not hold - each refused with its one message
TEST_F(CraftLearnActionTest, CanActRefusesWhatValidateNewRecipeRefuses) {
	setRecipes({ROAST_ININA_RECIPE});
	const std::string morphName = dataholders::DataManager::SKILL_DATA->getSkillTemplate(MORPH)->getL10n();

	EXPECT_FALSE(actionOf(RECIPE_ROAST_ININA).canAct(player(), nullptr, nullptr));
	EXPECT_FALSE(actionOf(RECIPE_ROAST_CONIDE).canAct(player(), nullptr, nullptr));
	EXPECT_FALSE(actionOf(MORPH_METHOD_ININA).canAct(player(), nullptr, nullptr));
	EXPECT_FALSE(actionOf(DESIGN_METAL_PLATE).canAct(player(), nullptr, nullptr));

	EXPECT_EQ(sent(), cp::exactly({message(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARNED_ALREADY()),
						  message(SM_SYSTEM_MESSAGE::STR_CRAFTRECIPE_RACE_CHECK()),
						  message(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_CANT_LEARN_SKILL(morphName)),
						  message(SM_SYSTEM_MESSAGE::STR_RECIPEITEM_CANT_USE_NO_RECIPE())}));
}

// ------------------------------------------------------------------------------------------------------------------------------------------ act

// CraftLearnAction.java:26-35: the item is used up (Storage.decreaseByObjectId: SM_DELETE_ITEM with ItemDeleteType.USE 0x17 and the cube size,
// ItemPacketService.java:119, :178-185), STR_USE_ITEM(item), RecipeList.addRecipe's SM_LEARN_RECIPE, RecipeService.addRecipe's
// STR_CRAFT_RECIPE_LEARN(recipe, name), and the three-argument SM_ITEM_USAGE_ANIMATION (time 0, end 1, unk3 1) to the player. Every packet is
// PacketSendUtility.sendPacket to the player (CraftLearnAction.java:29, :32-33): a player who knows him (a watcher in his known list, whom a
// broadcast would reach) is sent nothing
TEST_F(CraftLearnActionTest, ActUsesTheItemUpAndLearnsTheRecipe) {
	CRAFT_LEARN_REQUIRE_DATABASE();
	Item& recipe = stored(RECIPE_ITEM, RECIPE_ROAST_ININA, 1);
	cp::PlayerFixture watcher = cp::makePlayer(700102, 9802, "Watcher");
	cp::TestClient watcherClient;
	watcherClient.enterWorld(watcher);
	ASSERT_TRUE(f.knownList().addForTest(*watcher.player));
	clearSent(); // the player's SM_PLAYER_INFO of the watcher

	actionOf(RECIPE_ROAST_ININA).act(player(), Ptr<Item>(recipe), nullptr);

	EXPECT_EQ(sent(), cp::exactly({deleteItem(RECIPE_ITEM, 0x17), cubeSize(StorageType::CUBE, 0),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(recipe.getL10n())), recipeLearned(ROAST_ININA_RECIPE),
						  message(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARN(ROAST_ININA_RECIPE, "Looter")),
						  itemUsageAnimation(player().getObjectId(), RECIPE_ITEM, RECIPE_ROAST_ININA, 0, 1, 1)}));
	EXPECT_TRUE(watcherClient->sentBytes().empty()) << "the watcher is sent nothing";
	EXPECT_FALSE(player().getInventory().getItemByObjId(RECIPE_ITEM));
	EXPECT_TRUE(knowsRecipe(ROAST_ININA_RECIPE));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ROAST_ININA_RECIPE}));
	watcher.player->setClientConnection(nullptr);
}

// CraftLearnAction.java:31: addRecipe(player, recipeid, false) does not validate again - act on its own teaches even the Asmodian recipe to the
// Elyos player (CM_USE_ITEM asks canAct first, CM_USE_ITEM.java:98-114)
TEST_F(CraftLearnActionTest, ActDoesNotValidateTheRecipeAgain) {
	CRAFT_LEARN_REQUIRE_DATABASE();
	Item& recipe = stored(RECIPE_ITEM, RECIPE_ROAST_CONIDE, 1);

	actionOf(RECIPE_ROAST_CONIDE).act(player(), Ptr<Item>(recipe), nullptr);

	EXPECT_TRUE(knowsRecipe(ROAST_CONIDE_RECIPE));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ROAST_CONIDE_RECIPE}));
	const std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 6u);
	EXPECT_EQ(packets[3], recipeLearned(ROAST_CONIDE_RECIPE));
	EXPECT_EQ(packets[5], itemUsageAnimation(player().getObjectId(), RECIPE_ITEM, RECIPE_ROAST_CONIDE, 0, 1, 1));
}

// CraftLearnAction.java:28-34: one item of a stack is used (SM_INVENTORY_UPDATE_ITEM with DEC_ITEM_USE), STR_USE_ITEM; a recipe already known is
// not learnt again (RecipeList.addRecipe returns false before the DAO, RecipeList.java:31), so neither SM_LEARN_RECIPE nor the animation follows,
// and there is no validation message either (useValidation false)
TEST_F(CraftLearnActionTest, ActOnAKnownRecipeUsesOneItemAndLearnsNothing) {
	setRecipes({ROAST_ININA_RECIPE});
	Item& recipes = stored(RECIPE_ITEM, RECIPE_ROAST_ININA, 3);

	actionOf(RECIPE_ROAST_ININA).act(player(), Ptr<Item>(recipes), nullptr);

	EXPECT_EQ(recipes.getItemCount(), 2);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_UPDATE_ITEM(player(), recipes, ItemPacketService::ItemUpdateType::DEC_ITEM_USE)),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(recipes.getL10n()))}));
	EXPECT_TRUE(knowsRecipe(ROAST_ININA_RECIPE));
}

// RecipeService.java:57-61: addRecipe without validation takes the template straight from RECIPE_DATA - a recipe it does not hold is no
// recipe: the item is used up and said so, nothing is learnt and nothing more is sent
TEST_F(CraftLearnActionTest, ActOnARecipeTheDataDoesNotHoldUsesTheItemUpAndLearnsNothing) {
	Item& design = stored(RECIPE_ITEM, DESIGN_METAL_PLATE, 1);

	actionOf(DESIGN_METAL_PLATE).act(player(), Ptr<Item>(design), nullptr);

	EXPECT_EQ(sent(), cp::exactly({deleteItem(RECIPE_ITEM, 0x17), cubeSize(StorageType::CUBE, 0),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(design.getL10n()))}));
	EXPECT_EQ(player().getRecipeList()->size(), 0);
}

// CraftLearnAction.java:27-28: decreaseByObjectId(parentItem.getObjectId(), 1) uses the stack the player used, not the first of its item id -
// of two stacks of three the used one keeps two, each time (a known recipe: nothing is learnt, so no database is needed)
TEST_F(CraftLearnActionTest, ActUsesUpTheStackThePlayerUsed) {
	setRecipes({ROAST_ININA_RECIPE});
	Item& first = stored(RECIPE_ITEM, RECIPE_ROAST_ININA, 3);
	Item& second = stored(RECIPE_ITEM + 1, RECIPE_ROAST_ININA, 3);

	actionOf(RECIPE_ROAST_ININA).act(player(), Ptr<Item>(second), nullptr);

	EXPECT_EQ(first.getItemCount(), 3);
	EXPECT_EQ(second.getItemCount(), 2);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_UPDATE_ITEM(player(), second, ItemPacketService::ItemUpdateType::DEC_ITEM_USE)),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(second.getL10n()))}));

	clearSent();
	actionOf(RECIPE_ROAST_ININA).act(player(), Ptr<Item>(first), nullptr);

	EXPECT_EQ(first.getItemCount(), 2);
	EXPECT_EQ(second.getItemCount(), 2);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_UPDATE_ITEM(player(), first, ItemPacketService::ItemUpdateType::DEC_ITEM_USE)),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(first.getL10n()))}));
}

// CraftLearnAction.java:27-28: an item that is not in the cube cannot be used up - nothing else happens, even while the cube holds a stack of the
// same item id (decreaseByObjectId looks the used item up by its object id)
TEST_F(CraftLearnActionTest, ActWithAnItemOutsideTheCubeDoesNothing) {
	Item& recipe = loose(RECIPE_ITEM, RECIPE_ROAST_ININA, 1);
	Item& inTheCube = stored(RECIPE_ITEM + 1, RECIPE_ROAST_ININA, 1);

	actionOf(RECIPE_ROAST_ININA).act(player(), Ptr<Item>(recipe), nullptr);

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(inTheCube.getItemCount(), 1);
	EXPECT_TRUE(player().getInventory().getItemByObjId(RECIPE_ITEM + 1));
	EXPECT_FALSE(knowsRecipe(ROAST_ININA_RECIPE));
}

// CraftLearnAction.java:28: act dereferences the item first - a null is Java's NullPointerException, before anything is sent
TEST_F(CraftLearnActionTest, ActWithoutAnItemIsJavasNullPointerException) {
	EXPECT_THROW(actionOf(RECIPE_ROAST_ININA).act(player(), nullptr, nullptr), runtime::NullPointerException);
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace aion::gameserver::services::item::test
