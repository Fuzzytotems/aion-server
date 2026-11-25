#include "aion/gameserver/model/gameobjects/player/RecipeList.h"

#include "aion/gameserver/dao/PlayerRecipesDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_DELETE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player {

RecipeList::RecipeList() = default;

RecipeList::~RecipeList() = default;

runtime::Ref<RecipeList> RecipeList::create() {
	return runtime::makeRef<RecipeList>();
}

runtime::Ref<RecipeList> RecipeList::create(const std::unordered_set<int32_t>& recipeIds) {
	runtime::Ref<RecipeList> recipeList = create();
	for (int32_t recipeId : recipeIds)
		recipeList->addRecipe(recipeId, 0);
	return recipeList;
}

int32_t RecipeList::getMaxProductionCount(int32_t recipeId) {
	// Java RecipeData.java:51-54
	const templates::recipe::RecipeTemplate* recipeTemplate = dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId);
	return recipeTemplate == nullptr || !recipeTemplate->getMaxProductionCount() ? 0 : *recipeTemplate->getMaxProductionCount();
}

std::unordered_map<int32_t, int32_t> RecipeList::getRecipes() {
	SYNCHRONIZED(*this) {
		std::unordered_map<int32_t, int32_t> recipes;
		for (const auto& entry : productionLimitByRecipeId.snapshot())
			recipes.emplace(entry.key, entry.value);
		return recipes;
	}
}

bool RecipeList::addRecipe(Player& player, int32_t recipeId) {
	return addRecipe(runtime::Ptr<Player>(&player), recipeId, getMaxProductionCount(recipeId));
}

bool RecipeList::addRecipe(int32_t recipeId, int32_t productionLimit) {
	return addRecipe(runtime::Ptr<Player>(), recipeId, productionLimit);
}

bool RecipeList::addRecipe(runtime::Ptr<Player> player, int32_t recipeId, int32_t productionLimit) {
	SYNCHRONIZED(*this) {
		if (productionLimitByRecipeId.putIfAbsent(recipeId, productionLimit))
			return false;
		if (player) {
			if (!dao::PlayerRecipesDAO::addRecipe(player->getObjectId(), recipeId, productionLimit)) {
				productionLimitByRecipeId.remove(recipeId);
				return false;
			}
			// Java 68eccc7b1 sends SM_LEARN_RECIPE(recipeId, productionLimit); the packet's count byte is wave 5 (e5c133836), it still writes 0
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_LEARN_RECIPE(recipeId));
		}
		return true;
	}
}

bool RecipeList::deleteRecipe(Player& player, int32_t recipeId) {
	SYNCHRONIZED(*this) {
		if (productionLimitByRecipeId.containsKey(recipeId) && dao::PlayerRecipesDAO::delRecipe(player.getObjectId(), recipeId)) {
			productionLimitByRecipeId.remove(recipeId);
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_RECIPE_DELETE(recipeId));
			return true;
		}
		return false;
	}
}

bool RecipeList::isRecipePresent(int32_t recipeId) {
	SYNCHRONIZED(*this) {
		return productionLimitByRecipeId.containsKey(recipeId);
	}
}

int32_t RecipeList::size() {
	SYNCHRONIZED(*this) {
		return productionLimitByRecipeId.size();
	}
}

} // namespace aion::gameserver::model::gameobjects::player
