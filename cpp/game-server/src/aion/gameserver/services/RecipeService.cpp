#include "aion/gameserver/services/RecipeService.h"

#include <string>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

const model::templates::recipe::RecipeTemplate* RecipeService::validateNewRecipe(model::gameobjects::player::Player& player, int32_t recipeId) {
	if (player.getRecipeList()->size() >= 1600) {
		PacketSendUtility::sendMessage(player, "You are unable to have more than 1600 recipes at the same time.");
		return nullptr;
	}

	const model::templates::recipe::RecipeTemplate* template_ = dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId);
	if (template_ == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_RECIPEITEM_CANT_USE_NO_RECIPE());
		return nullptr;
	}

	// Java compares the enum references: a recipe without a race is neither PC_ALL nor the player's race
	if (template_->getRace() != model::Race::PC_ALL) {
		if (template_->getRace() != player.getRace()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFTRECIPE_RACE_CHECK());
			return nullptr;
		}
	}

	if (player.getRecipeList()->isRecipePresent(recipeId)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARNED_ALREADY());
		return nullptr;
	}

	if (!player.getSkillList()->isSkillPresent(template_->getSkillId())) {
		const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(template_->getSkillId());
		if (skillTemplate == nullptr) // Java: NullPointerException at getSkillTemplate(skillId).getL10n()
			throw runtime::NullPointerException("SKILL_DATA.getSkillTemplate(" + std::to_string(template_->getSkillId()) + ") is null");
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_CANT_LEARN_SKILL(skillTemplate->getL10n()));
		return nullptr;
	}

	if (template_->getSkillpoint() > player.getSkillList()->getSkillLevel(template_->getSkillId())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_CANT_LEARN_SKILLPOINT());
		return nullptr;
	}

	return template_;
}

bool RecipeService::addRecipe(model::gameobjects::player::Player& player, int32_t recipeId, bool useValidation) {
	const model::templates::recipe::RecipeTemplate* template_ = nullptr;
	if (useValidation)
		template_ = validateNewRecipe(player, recipeId);
	else
		template_ = dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId);

	if (template_ == nullptr)
		return false;

	if (player.getRecipeList()->addRecipe(player, recipeId)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CRAFT_RECIPE_LEARN(recipeId, player.getName()));
		return true;
	}
	return false;
}

void RecipeService::autoLearnRecipes(model::gameobjects::player::Player& player, int32_t skillId, int32_t skillLvl) {
	for (const model::templates::recipe::RecipeTemplate* recipe :
		dataholders::DataManager::RECIPE_DATA->getAutolearnRecipes(player.getRace(), skillId, skillLvl))
		player.getRecipeList()->addRecipe(player, recipe->getId());
}

} // namespace aion::gameserver::services
