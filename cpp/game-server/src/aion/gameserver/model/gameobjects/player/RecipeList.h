#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.recipeList`), created with create(). The storage is
 * the one of Java 68eccc7b1 (e5c133836: the remaining production count per recipe, stored in player_recipes.production_limit); its use by
 * crafting (decreaseProductionLimit) and the packets that send the counts are wave 5 of the upstream sync (docs/deviations/P4-12.md).
 *
 * @author MrPoke, Neon, SVDNESS
 */
class RecipeList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** Recipe ID -> remaining production count (0 = unlimited) */
	runtime::HashMap<int32_t, int32_t> productionLimitByRecipeId{AION_LOCK_CLASS(RecipeList::productionLimitByRecipeId)};

protected:
	RecipeList();
	~RecipeList() override;

public:
	/** Java: new RecipeList() */
	static runtime::Ref<RecipeList> create();

	/** C++ only (test fixtures): a list of the given recipes, each unlimited (addRecipe(recipeId, 0)) */
	static runtime::Ref<RecipeList> create(const std::unordered_set<int32_t>& recipeIds);

	/**
	 * Java DataManager.RECIPE_DATA.getMaxProductionCount(recipeId) (RecipeData.java:51, e5c133836): the template's max_production_count, 0 for
	 * none or an unknown recipe. C++ only until the data holder gains it (the sync's data holders belong to its static-data lane).
	 */
	static int32_t getMaxProductionCount(int32_t recipeId);

	/** synchronized. Java Map.copyOf */
	std::unordered_map<int32_t, int32_t> getRecipes();

	bool addRecipe(Player& player, int32_t recipeId);

	/** @param productionLimit the allowed number of usages (0 = unlimited) */
	bool addRecipe(int32_t recipeId, int32_t productionLimit);

	/** synchronized. A null player only adds the recipe to the list (Java addRecipe(null, recipeId, productionLimit)) */
	bool addRecipe(runtime::Ptr<Player> player, int32_t recipeId, int32_t productionLimit);

	/** synchronized */
	bool deleteRecipe(Player& player, int32_t recipeId);

	/** synchronized */
	bool isRecipePresent(int32_t recipeId);

	/** synchronized */
	int32_t size();
};

} // namespace aion::gameserver::model::gameobjects::player
