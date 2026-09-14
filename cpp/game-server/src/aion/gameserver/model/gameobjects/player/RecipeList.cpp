#include "aion/gameserver/model/gameobjects/player/RecipeList.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

RecipeList::RecipeList(const std::unordered_set<int32_t>& recipeListValue) {
	// Java: this.recipeList = recipeList
	for (int32_t recipeId : recipeListValue)
		recipeList.add(recipeId);
}

RecipeList::RecipeList() = default;

RecipeList::~RecipeList() = default;

runtime::Ref<RecipeList> RecipeList::create(const std::unordered_set<int32_t>& recipeListValue) {
	return runtime::makeRef<RecipeList>(recipeListValue);
}

runtime::Ref<RecipeList> RecipeList::create() {
	return runtime::makeRef<RecipeList>();
}

bool RecipeList::addRecipe(Player& player, int32_t recipeId) {
	AION_UNPORTED();
}

bool RecipeList::deleteRecipe(Player& player, int32_t recipeId) {
	AION_UNPORTED();
}

bool RecipeList::isRecipePresent(int32_t recipeId) {
	AION_UNPORTED();
}

int32_t RecipeList::size() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
