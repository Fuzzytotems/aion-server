#include "aion/gameserver/model/gameobjects/player/RecipeList.h"

#include "aion/gameserver/dao/PlayerRecipesDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_DELETE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

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
	if (!isRecipePresent(recipeId) && dao::PlayerRecipesDAO::addRecipe(player.getObjectId(), recipeId)) {
		recipeList.add(recipeId);
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_LEARN_RECIPE(recipeId));
		return true;
	}
	return false;
}

bool RecipeList::deleteRecipe(Player& player, int32_t recipeId) {
	if (recipeList.contains(recipeId) && dao::PlayerRecipesDAO::delRecipe(player.getObjectId(), recipeId)) {
		recipeList.remove(recipeId);
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RECIPE_DELETE(recipeId));
		return true;
	}
	return false;
}

bool RecipeList::isRecipePresent(int32_t recipeId) {
	return recipeList.contains(recipeId);
}

int32_t RecipeList::size() {
	return recipeList.size();
}

} // namespace aion::gameserver::model::gameobjects::player
