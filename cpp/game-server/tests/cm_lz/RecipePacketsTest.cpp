// M5c C-04 and C-05 (m5c-plan.md §5, §20.3; P5-16, and P5-07's CraftLearnAction): CM_RECIPE_DELETE (C_RECIPE_DELETE), what the client sends
// when the player deletes a recipe from his recipe book, and CM_USE_ITEM on a recipe item, whose one action is CraftLearnAction (§2.6 row 6:
// before C-05 both of its bodies were AION_UNPORTED stubs that CM_USE_ITEM reached).
//
// Java: CM_RECIPE_DELETE.java:21-29 (RecipeList.deleteRecipe, RecipeList.java:40-47); CM_USE_ITEM.java:58-124 with CraftLearnAction.java:26-40
// (RecipeService.validateNewRecipe and addRecipe, RecipeService.java:15-70). The read cases lay the body out from the Java readImpl; the run
// cases drive runImpl on CraftPacketTestSupport.h's fixture. The recipe list writes through PlayerRecipesDAO, so the cases whose Java flow needs
// the write to succeed use the economy test database (CRAFT_PACKET_REQUIRE_DATABASE), and the cases without one observe Java's flow on a failed
// write: DB.insertUpdate returns false and the list keeps what it had. The rows are verbatim (CraftPacketTestSupport.h): Recipe: Roast Inina
// (item_templates.xml:754001, recipe 155001381, cooking 1, ELYOS).

#include "../cm_ak/CraftPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/gameserver/network/aion/clientpackets/CM_RECIPE_DELETE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_RECIPE_DELETE_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_RECIPE_DELETE.h declares: the field readImpl decoded, which Java keeps package-private */
struct CM_RECIPE_DELETETestAccess {
	static int32_t recipeId(const CM_RECIPE_DELETE& p) { return p.recipeId; }
};

namespace testing::items {
namespace {

using serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t RECIPE_ITEM = 830101; // the recipe item's object id

// ------------------------------------------------------------------------------------------------------------------------------------ readImpl

// CM_RECIPE_DELETE.java:21-23: readD recipeId (the gate's X21 body, GameSession::buildCM_RECIPE_DELETE(155001381))
TEST(RecipeDeleteReadTest, TheBodyIsTheRecipeId) {
	int32_t unread = -1;
	auto p = readAlone<CM_RECIPE_DELETE>(CM_RECIPE_DELETE_OPCODE, {0x25, 0x22, 0x3D, 0x09}, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_RECIPE_DELETETestAccess::recipeId(*p), ROAST_ININA_RECIPE);
	EXPECT_EQ(unread, 0);

	p = readAlone<CM_RECIPE_DELETE>(CM_RECIPE_DELETE_OPCODE, {0xFE, 0xFF, 0xFF, 0xFF, 0x01}, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_RECIPE_DELETETestAccess::recipeId(*p), -2) << "a signed int";
	EXPECT_EQ(unread, 1);
}

TEST(RecipeDeleteReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_RECIPE_DELETE*>(
				  CM_RECIPE_DELETE_clientPacketFactory(CM_RECIPE_DELETE_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_EQ(economyTableEntries("CM_RECIPE_DELETE", CM_RECIPE_DELETE_OPCODE), 1);
}

// ------------------------------------------------------------------------------------------------------------------------ CM_RECIPE_DELETE runs

class RecipePacketsTest : public CraftPacketTest {
protected:
	void deleteRecipe(int32_t recipeId) { readAndRun<CM_RECIPE_DELETE>(CM_RECIPE_DELETE_OPCODE, PacketWriter().D(recipeId).data); }

	/** C_USE_ITEM of a cube item without a target: D item, C 0 (CM_USE_ITEM.java:38-52) */
	void use(int32_t itemObjId) { readAndRun<CM_USE_ITEM>(CM_USE_ITEM_OPCODE, PacketWriter().D(itemObjId).C(0).data); }

	/** A player_recipes row of the holder, as PlayerRecipesDAO.addRecipe writes it */
	static void storeRecipe(int32_t recipeId) {
		craftdb::execute("INSERT INTO player_recipes (player_id, recipe_id) VALUES (" + std::to_string(CRAFT_HOLDER_ID) + ", " +
			std::to_string(recipeId) + ")");
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }
};

// CM_RECIPE_DELETE.java:28 -> RecipeList.deleteRecipe (RecipeList.java:40-47): a known recipe leaves the list and player_recipes, and the
// client is told (SM_RECIPE_DELETE); the other recipe stays - the gate's X21
TEST_F(RecipePacketsTest, AKnownRecipeIsDeletedFromTheListAndTheDatabase) {
	CRAFT_PACKET_REQUIRE_DATABASE();
	setRecipes({ROAST_ININA_RECIPE, ININA_MORPH_RECIPE});
	storeRecipe(ROAST_ININA_RECIPE);
	storeRecipe(ININA_MORPH_RECIPE);

	deleteRecipe(ROAST_ININA_RECIPE);

	EXPECT_EQ(sent(), exactly({recipeDeleted(ROAST_ININA_RECIPE)}));
	EXPECT_FALSE(knowsRecipe(ROAST_ININA_RECIPE));
	EXPECT_TRUE(knowsRecipe(ININA_MORPH_RECIPE));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ININA_MORPH_RECIPE}));
}

// RecipeList.java:41: a recipe the list does not hold is not deleted - not even its database row
TEST_F(RecipePacketsTest, ARecipeTheListDoesNotHoldIsNotDeleted) {
	CRAFT_PACKET_REQUIRE_DATABASE();
	setRecipes({ININA_MORPH_RECIPE});
	storeRecipe(ROAST_ININA_RECIPE);
	storeRecipe(ININA_MORPH_RECIPE);

	deleteRecipe(ROAST_ININA_RECIPE);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(knowsRecipe(ININA_MORPH_RECIPE));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ININA_MORPH_RECIPE, ROAST_ININA_RECIPE}));
}

// RecipeList.java:41: without a database PlayerRecipesDAO.delRecipe returns false, so the recipe stays and the client is told nothing
TEST_F(RecipePacketsTest, WithoutADatabaseTheRecipeStays) {
	ASSERT_FALSE(commons::database::DatabaseFactory::isInitialized()) << "this case runs without a database";
	setRecipes({ROAST_ININA_RECIPE});

	deleteRecipe(ROAST_ININA_RECIPE);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(knowsRecipe(ROAST_ININA_RECIPE));
}

// CM_RECIPE_DELETE.java:27-28: Java checks no active player - a connection without one is its NullPointerException (which AionClientPacket.run
// logs)
TEST_F(RecipePacketsTest, WithoutAPlayerItIsJavasNullPointerException) {
	setRecipes({ROAST_ININA_RECIPE});
	TestClient loggedOut;
	EconomyDriver<CM_RECIPE_DELETE> packet(CM_RECIPE_DELETE_OPCODE);
	ASSERT_TRUE(packet.readOn(PacketWriter().D(ROAST_ININA_RECIPE).data, loggedOut.get()));

	EXPECT_THROW(packet.runNow(), runtime::NullPointerException);

	EXPECT_TRUE(knowsRecipe(ROAST_ININA_RECIPE));
	EXPECT_TRUE(sent().empty());
}

// ------------------------------------------------------------------------------------------------------- CM_USE_ITEM on a recipe item (C-05)

// CM_USE_ITEM.java:98-124 with CraftLearnAction: canAct (validateNewRecipe passes: cooking 1, the recipe not known), then act - the item is used
// up (Storage.decreaseByObjectId: SM_DELETE_ITEM with ItemDeleteType.USE 0x17 and the cube size), STR_USE_ITEM(item), RecipeList.addRecipe's
// SM_LEARN_RECIPE, RecipeService.addRecipe's STR_CRAFT_RECIPE_LEARN(recipe, name) and the three-argument SM_ITEM_USAGE_ANIMATION (time 0, end 1,
// unk 1); the recipe is in the list and in player_recipes
TEST_F(RecipePacketsTest, UsingARecipeItemTeachesItsRecipe) {
	CRAFT_PACKET_REQUIRE_DATABASE();
	Item& recipe = give(RECIPE_ITEM, RECIPE_ROAST_ININA, 1);

	use(RECIPE_ITEM);

	EXPECT_EQ(sent(), exactly({deleteItem(RECIPE_ITEM, 0x17), cubeSize(StorageType::CUBE, 0),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(recipe.getL10n())), recipeLearned(ROAST_ININA_RECIPE),
						  message(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARN(ROAST_ININA_RECIPE, "Holder")),
						  usageAnimation(player().getObjectId(), RECIPE_ITEM, RECIPE_ROAST_ININA, 0, 1, 1)}));
	EXPECT_EQ(countOf(RECIPE_ROAST_ININA), 0);
	EXPECT_TRUE(knowsRecipe(ROAST_ININA_RECIPE));
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ROAST_ININA_RECIPE}));
}

// CM_USE_ITEM.java:113-114: canAct refuses a known recipe (validateNewRecipe's STR_CRAFT_RECIPE_LEARNED_ALREADY), so act never runs and the
// item is kept
TEST_F(RecipePacketsTest, AKnownRecipeIsRefusedAndTheItemKept) {
	setRecipes({ROAST_ININA_RECIPE});
	give(RECIPE_ITEM, RECIPE_ROAST_ININA, 1);

	use(RECIPE_ITEM);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARNED_ALREADY())}));
	EXPECT_EQ(countOf(RECIPE_ROAST_ININA), 1);
}

// Without a database canAct passes and act uses the item up and says so, but RecipeList.addRecipe's PlayerRecipesDAO.addRecipe returns false:
// nothing is learned, and there is no SM_ITEM_USAGE_ANIMATION (CraftLearnAction.java:32-34) - Java's flow on a failed write
TEST_F(RecipePacketsTest, WithoutADatabaseTheItemIsUsedUpAndNothingLearned) {
	ASSERT_FALSE(commons::database::DatabaseFactory::isInitialized()) << "this case runs without a database";
	Item& recipe = give(RECIPE_ITEM, RECIPE_ROAST_ININA, 1);

	use(RECIPE_ITEM);

	EXPECT_EQ(sent(), exactly({deleteItem(RECIPE_ITEM, 0x17), cubeSize(StorageType::CUBE, 0),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(recipe.getL10n()))}));
	EXPECT_EQ(countOf(RECIPE_ROAST_ININA), 0);
	EXPECT_FALSE(knowsRecipe(ROAST_ININA_RECIPE));
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
