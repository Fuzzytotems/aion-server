#include "aion/gameserver/services/RecipeService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

const model::templates::recipe::RecipeTemplate* RecipeService::validateNewRecipe(model::gameobjects::player::Player& player, int32_t recipeId) {
	AION_UNPORTED();
}

bool RecipeService::addRecipe(model::gameobjects::player::Player& player, int32_t recipeId, bool useValidation) {
	AION_UNPORTED();
}

void RecipeService::autoLearnRecipes(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLvl) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
