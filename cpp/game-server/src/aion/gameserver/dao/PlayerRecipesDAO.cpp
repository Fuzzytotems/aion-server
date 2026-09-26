#include "aion/gameserver/dao/PlayerRecipesDAO.h"

#include <string_view>
#include <unordered_set>

#include "aion/commons/database/DB.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"

namespace aion::gameserver::dao {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT `recipe_id` FROM player_recipes WHERE `player_id`=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO player_recipes (`player_id`, `recipe_id`) VALUES (?, ?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM player_recipes WHERE `player_id`=? AND `recipe_id`=?";

} // namespace

runtime::Ref<model::gameobjects::player::RecipeList> PlayerRecipesDAO::load(int32_t playerId) {
	std::unordered_set<int32_t> recipeList;
	DB::select(
		SELECT_QUERY, [&](PreparedStatement& ps) { ps.setInt(1, playerId); },
		[&](ResultSet& rs) {
			while (rs.next()) {
				recipeList.insert(rs.getInt("recipe_id"));
			}
		});
	return model::gameobjects::player::RecipeList::create(recipeList);
}

bool PlayerRecipesDAO::addRecipe(int32_t playerId, int32_t recipeId) {
	return DB::insertUpdate(ADD_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, playerId);
		ps.setInt(2, recipeId);
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
