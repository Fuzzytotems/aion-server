#include "aion/gameserver/dao/PlayerRecipesDAO.h"

#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"

namespace aion::gameserver::dao {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT `recipe_id`, `production_limit` FROM player_recipes WHERE `player_id`=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO player_recipes (`player_id`, `recipe_id`, `production_limit`) VALUES (?, ?, ?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_recipes SET `production_limit`=? WHERE `player_id`=? AND `recipe_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM player_recipes WHERE `player_id`=? AND `recipe_id`=?";

} // namespace

runtime::Ref<model::gameobjects::player::RecipeList> PlayerRecipesDAO::load(int32_t playerId) {
	runtime::Ref<model::gameobjects::player::RecipeList> recipeList = model::gameobjects::player::RecipeList::create();
	DB::select(
		SELECT_QUERY, [&](PreparedStatement& ps) { ps.setInt(1, playerId); },
		[&](ResultSet& rs) {
			while (rs.next()) {
				int32_t recipeId = rs.getInt("recipe_id");
				int32_t productionLimit = rs.getInt("production_limit");
				if (productionLimit == 0) // recipes stored before production counts were tracked have a count of 0, they start with the full count
					productionLimit = model::gameobjects::player::RecipeList::getMaxProductionCount(recipeId);
				recipeList->addRecipe(recipeId, productionLimit);
			}
		});
	return recipeList;
}

bool PlayerRecipesDAO::addRecipe(int32_t playerId, int32_t recipeId, int32_t productionLimit) {
	return DB::insertUpdate(ADD_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, playerId);
		ps.setInt(2, recipeId);
		ps.setInt(3, productionLimit);
		ps.execute();
	});
}

bool PlayerRecipesDAO::updateProductionLimit(int32_t playerId, int32_t recipeId, int32_t productionLimit) {
	return DB::insertUpdate(UPDATE_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, productionLimit);
		ps.setInt(2, playerId);
		ps.setInt(3, recipeId);
		ps.execute();
	});
}

bool PlayerRecipesDAO::delRecipe(int32_t playerId, int32_t recipeId) {
	return DB::insertUpdate(DELETE_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, playerId);
		ps.setInt(2, recipeId);
		ps.execute();
	});
}

} // namespace aion::gameserver::dao
