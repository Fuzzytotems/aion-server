#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/recipe/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author KID, Neon
 */
class RecipeService {
public:
	static const model::templates::recipe::RecipeTemplate* validateNewRecipe(model::gameobjects::player::Player& player, int32_t recipeId);
	static bool addRecipe(model::gameobjects::player::Player& player, int32_t recipeId, bool useValidation);
	static void autoLearnRecipes(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLvl);
};

} // namespace aion::gameserver::services
