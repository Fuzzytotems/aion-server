// M5c C-01 / C-06 (m5c-plan.md §5, P5-09c): RecipeService - validateNewRecipe's checks in their order, addRecipe with and without them, and
// autoLearnRecipes' skill, level and race filter (the path of W-06: SkillLearnService.onLearnSkill of a crafting or morph skill). The fixture
// and its rules are CraftTestSupport.h's.
//
// Java: RecipeService.java:15-73, RecipeData.java:35-45. Expectations: `oracle.py m5c-craft --no-profile --set
// gameserver.event.service.disabled_events=* --skill ID --level N` (recipes.ELYOS / ASMODIANS: autolearn, newAtLevel) and `--recipe ID`
// (learn.recipeItemRule) where named, else the Java lines beside each assertion.

#include "CraftTestSupport.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/RecipeService.h"

namespace aion::gameserver::economy::test::craft {
namespace {

using network::aion::serverpackets::SM_LEARN_RECIPE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::RecipeService;

class RecipeServiceTest : public CraftTest {
protected:
	const model::templates::recipe::RecipeTemplate* validate(int32_t recipeId) { return RecipeService::validateNewRecipe(player(), recipeId); }

	const model::templates::recipe::RecipeTemplate* recipe(int32_t recipeId) {
		return dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId);
	}

	/** `count` recipe ids that are no recipe of RECIPE_TEMPLATES_XML (RecipeList only counts them) */
	void knowRecipes(int32_t count) {
		std::unordered_set<int32_t> ids;
		for (int32_t id = 1; id <= count; id++)
			ids.insert(id);
		player().setRecipeList(model::gameobjects::player::RecipeList::create(ids));
	}

	/** The recipe ids of the captured SM_LEARN_RECIPE packets (SM_LEARN_RECIPE.java: D(recipeId)), in order */
	std::vector<int32_t> learnedRecipes() {
		std::vector<int32_t> ids;
		for (const std::vector<uint8_t>& packet : itemtest::packetsOf(sent(), SM_LEARN_RECIPE_OPCODE)) {
			EXPECT_EQ(packet, serializedFor(SM_LEARN_RECIPE(PacketReader(cp::bodyOf(packet)).D())));
			ids.push_back(PacketReader(cp::bodyOf(packet)).D());
		}
		return ids;
	}
};

// :15-51: every refusal is the first check that fails - each step removes the one failure the step before reported and keeps all later ones
// - and returns null after its message; the template when all pass, with nothing sent
TEST_F(RecipeServiceTest, ValidateNewRecipeChecksInJavasOrder) {
	// :16-19: 1600 known recipes, whatever else is wrong (an unknown id)
	knowRecipes(1600);
	EXPECT_EQ(validate(155999999), nullptr);
	EXPECT_EQ(sent(), cp::exactly({serializedFor(network::aion::serverpackets::SM_MESSAGE(0, "",
						  "You are unable to have more than 1600 recipes at the same time.", model::ChatType::GOLDEN_YELLOW))}));
	clearSent();

	// :21-25: 1599 are fine; an id without a template
	knowRecipes(1599);
	EXPECT_EQ(validate(155999999), nullptr);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_RECIPEITEM_CANT_USE_NO_RECIPE())}));
	clearSent();

	// :27-32: the Asmodian cooking recipe for an Elyos (who also knows it already and lacks the skill)
	setRecipes({ASMODIAN_COOKING_RECIPE});
	EXPECT_EQ(validate(ASMODIAN_COOKING_RECIPE), nullptr);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFTRECIPE_RACE_CHECK())}));
	clearSent();

	// :34-37: known already (and no skill)
	setRecipes({ROAST_ININA_RECIPE});
	EXPECT_EQ(validate(ROAST_ININA_RECIPE), nullptr);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARNED_ALREADY())}));
	clearSent();
	setRecipes({});

	// :39-43: no cooking skill - the message names it; :45-48: cooking below the recipe's skillpoint 1
	EXPECT_EQ(validate(ROAST_ININA_RECIPE), nullptr);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_CANT_LEARN_SKILL(javaL10n(COOKING_NAME)))}));
	clearSent();
	setSkills({{COOKING, 0}});
	EXPECT_EQ(validate(ROAST_ININA_RECIPE), nullptr);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_CANT_LEARN_SKILLPOINT())}));
	clearSent();

	setSkills({{COOKING, 1}});
	EXPECT_EQ(validate(ROAST_ININA_RECIPE), recipe(ROAST_ININA_RECIPE));
	EXPECT_TRUE(sent().empty());
}

// :27-32: a PC_ALL recipe passes the race check for both races (the pistol recipe, weaponsmithing 1), a race's own recipe for its race only
TEST_F(RecipeServiceTest, APcAllRecipeIsForEitherRace) {
	setSkills({{WEAPONSMITHING, 1}, {COOKING, 1}});
	EXPECT_EQ(validate(PISTOL_RECIPE), recipe(PISTOL_RECIPE));
	EXPECT_EQ(validate(ROAST_ININA_RECIPE), recipe(ROAST_ININA_RECIPE));
	commonData().setRace(model::Race::ASMODIANS);
	EXPECT_EQ(validate(PISTOL_RECIPE), recipe(PISTOL_RECIPE));
	EXPECT_EQ(validate(ASMODIAN_COOKING_RECIPE), recipe(ASMODIAN_COOKING_RECIPE));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(validate(ROAST_ININA_RECIPE), nullptr);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFTRECIPE_RACE_CHECK())}));
}

// :53-68 without the database: the validation refuses with its message; without it (or an unknown id) nothing is sent, and RecipeList.addRecipe
// fails at PlayerRecipesDAO, so addRecipe answers false without STR_CRAFT_RECIPE_LEARN
TEST_F(RecipeServiceTest, AddRecipeValidatesOnlyWhenAsked) {
	setSkills({{COOKING, 1}});
	EXPECT_FALSE(RecipeService::addRecipe(player(), ASMODIAN_COOKING_RECIPE, true));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFTRECIPE_RACE_CHECK())}));
	clearSent();
	EXPECT_FALSE(RecipeService::addRecipe(player(), 155999999, false));
	EXPECT_FALSE(RecipeService::addRecipe(player(), ASMODIAN_COOKING_RECIPE, false));
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(knowsRecipe(ASMODIAN_COOKING_RECIPE));
}

// :53-68 with the database: the recipe is stored and announced (SM_LEARN_RECIPE from RecipeList, then STR_CRAFT_RECIPE_LEARN with the recipe id
// and the player's name); without the validation even another race's recipe; a known recipe is not learned twice
TEST_F(RecipeServiceTest, AddRecipeStoresAndAnnouncesTheRecipe) {
	CRAFT_REQUIRE_DATABASE();
	setSkills({{COOKING, 1}});
	EXPECT_TRUE(RecipeService::addRecipe(player(), ROAST_ININA_RECIPE, true));
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_LEARN_RECIPE(ROAST_ININA_RECIPE)),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARN(ROAST_ININA_RECIPE, "Holder"))}));
	clearSent();
	EXPECT_TRUE(RecipeService::addRecipe(player(), ASMODIAN_COOKING_RECIPE, false));
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_LEARN_RECIPE(ASMODIAN_COOKING_RECIPE)),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARN(ASMODIAN_COOKING_RECIPE, "Holder"))}));
	clearSent();
	EXPECT_FALSE(RecipeService::addRecipe(player(), ROAST_ININA_RECIPE, false));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(storedRecipes(), (std::vector<int32_t>{ROAST_ININA_RECIPE, ASMODIAN_COOKING_RECIPE}));
}

// :70-73 with RecipeData.getAutolearnRecipes (--skill 40009 --level 1: recipes.ELYOS.newAtLevel 155000001, 155000002, 155000005): the autolearn
// recipes of the skill up to the level, of the player's race or PC_ALL, in document order - not the morph recipe without autolearn (155000003),
// not the Asmodian ones (the gate's X21a); cooking 1 learns only its Elyos recipe; weaponsmithing 450 adds the level-450 recipe
TEST_F(RecipeServiceTest, AutoLearnRecipesLearnsTheSkillsRecipesUpToTheLevelForTheRace) {
	CRAFT_REQUIRE_DATABASE();
	RecipeService::autoLearnRecipes(player(), MORPH, 1);
	EXPECT_EQ(learnedRecipes(), (std::vector<int32_t>{155000001, 155000002, 155000005}));
	clearSent();
	RecipeService::autoLearnRecipes(player(), COOKING, 1);
	EXPECT_EQ(learnedRecipes(), (std::vector<int32_t>{ROAST_ININA_RECIPE}));
	clearSent();
	RecipeService::autoLearnRecipes(player(), WEAPONSMITHING, 449);
	EXPECT_EQ(learnedRecipes(), (std::vector<int32_t>{STEEL_INGOT_RECIPE}));
	clearSent();
	RecipeService::autoLearnRecipes(player(), WEAPONSMITHING, 450);
	EXPECT_EQ(learnedRecipes(), (std::vector<int32_t>{WEAPONSMITH_450_RECIPE})) << "the known one is not learned again";
	EXPECT_EQ(storedRecipes(),
		(std::vector<int32_t>{155000001, 155000002, 155000005, STEEL_INGOT_RECIPE, ROAST_ININA_RECIPE, WEAPONSMITH_450_RECIPE}));
}

// the same for an Asmodian (--skill 40009 --level 1: recipes.ASMODIANS.newAtLevel 155005001, 155005002, 155005005; --skill 40001 --level 1)
TEST_F(RecipeServiceTest, AutoLearnRecipesLearnsTheAsmodianRecipesForAnAsmodian) {
	CRAFT_REQUIRE_DATABASE();
	commonData().setRace(model::Race::ASMODIANS);
	RecipeService::autoLearnRecipes(player(), MORPH, 1);
	EXPECT_EQ(learnedRecipes(), (std::vector<int32_t>{155005001, 155005002, 155005005}));
	clearSent();
	RecipeService::autoLearnRecipes(player(), COOKING, 1);
	EXPECT_EQ(learnedRecipes(), (std::vector<int32_t>{ASMODIAN_COOKING_RECIPE}));
	clearSent();
	RecipeService::autoLearnRecipes(player(), WEAPONSMITHING, 450);
	EXPECT_TRUE(learnedRecipes().empty()) << "both weaponsmithing autolearn recipes of the rows are Elyos";
}

} // namespace
} // namespace aion::gameserver::economy::test::craft
