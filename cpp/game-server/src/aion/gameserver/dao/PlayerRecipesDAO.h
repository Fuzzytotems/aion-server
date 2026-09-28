#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author lord_rex
 */
class PlayerRecipesDAO {
public:
	static runtime::Ref<model::gameobjects::player::RecipeList> load(int32_t playerId);
	static bool addRecipe(int32_t playerId, int32_t recipeId);
	static bool delRecipe(int32_t playerId, int32_t recipeId);
};

} // namespace aion::gameserver::dao
