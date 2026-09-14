#include "aion/gameserver/dao/PlayerRecipesDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at PlayerRecipesDAO.java:24 (com.aionemu.gameserver.dao.PlayerRecipesDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at PlayerRecipesDAO.java:42 (com.aionemu.gameserver.dao.PlayerRecipesDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at PlayerRecipesDAO.java:54 (com.aionemu.gameserver.dao.PlayerRecipesDAO$3); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT `recipe_id` FROM player_recipes WHERE `player_id`=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO player_recipes (`player_id`, `recipe_id`) VALUES (?, ?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM player_recipes WHERE `player_id`=? AND `recipe_id`=?";

} // namespace

runtime::Ref<model::gameobjects::player::RecipeList> PlayerRecipesDAO::load(int32_t playerId) {
	AION_UNPORTED();
}

bool PlayerRecipesDAO::addRecipe(int32_t playerId, int32_t recipeId) {
	AION_UNPORTED();
}

bool PlayerRecipesDAO::delRecipe(int32_t playerId, int32_t recipeId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
