#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"

#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/model/craft/CraftQuestsLists.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::craft {

using utils::PacketSendUtility;

bool RelinquishCraftStatus::relinquishExpertStatus(model::gameobjects::player::Player& player, model::craft::Profession profession) {
	return relinquishExpertStatus(player, profession, expertPrice);
}

bool RelinquishCraftStatus::relinquishExpertStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t price) {
	return relinquishCraftStatus(player, profession, expertMinValue, expertMaxValue, price);
}

bool RelinquishCraftStatus::relinquishMasterStatus(model::gameobjects::player::Player& player, model::craft::Profession profession) {
	return relinquishMasterStatus(player, profession, masterPrice);
}

bool RelinquishCraftStatus::relinquishMasterStatus(model::gameobjects::player::Player& player, model::craft::Profession profession, int32_t price) {
	return relinquishCraftStatus(player, profession, masterMinValue, masterMaxValue, price);
}

bool RelinquishCraftStatus::relinquishCraftStatus(model::gameobjects::player::Player& player, model::craft::Profession profession,
	int32_t minSkillLevel, int32_t maxSkillLevel, int32_t price) {
	// Java: profession == null (the enum parameter is never null here)
	if (!model::craft::isCrafting(profession))
		return false;
	runtime::Ptr<model::skill::PlayerSkillEntry> skill = player.getSkillList()->getSkillEntry(model::craft::getSkillId(profession));
	if (!skill || skill->getSkillLevel() < minSkillLevel || skill->getSkillLevel() > maxSkillLevel)
		return false;
	if (!decreaseKinah(player, price))
		return false;
	skill->setSkillLvl(minSkillLevel - 1);
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SKILL_LIST(*skill, skillMessageId));
	removeRecipesAbove(player, skill->getSkillId(), minSkillLevel);
	deleteCraftStatusQuests(skill->getSkillId(), player, maxSkillLevel < masterMinValue);
	return true;
}

bool RelinquishCraftStatus::decreaseKinah(model::gameobjects::player::Player& player, int32_t basePrice) {
	if (basePrice > 0 && !player.getInventory().tryDecreaseKinah(trade::PricesService::getPriceForService(basePrice, player.getRace()))) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		return false;
	}
	return true;
}

void RelinquishCraftStatus::removeRecipesAbove(model::gameobjects::player::Player& player, int32_t skillId, int32_t level) {
	for (const model::templates::recipe::RecipeTemplate* recipe : dataholders::DataManager::RECIPE_DATA->getRecipeTemplates()) {
		if (recipe->getSkillId() != skillId || recipe->getSkillpoint() < level) {
			continue;
		}
		player.getRecipeList()->deleteRecipe(player, recipe->getId());
	}
}

void RelinquishCraftStatus::deleteCraftStatusQuests(int32_t skillId, model::gameobjects::player::Player& player, bool isExpert) {
	for (int32_t questId : model::craft::getMasterQuestIds(skillId, player.getRace())) {
		player.getQuestStateList()->deleteQuest(questId);
	}
	if (isExpert) {
		for (int32_t questId : model::craft::getExpertQuestIds(skillId, player.getRace())) {
			player.getQuestStateList()->deleteQuest(questId);
		}
	}
	questEngine::QuestEngine::getInstance().sendCompletedQuests(player);
	player.getController().updateNearbyQuests();
}

void RelinquishCraftStatus::removeExcessCraftStatus(model::gameobjects::player::Player& player, bool isExpert) {
	int32_t minValue = isExpert ? expertMinValue : masterMinValue;
	int32_t maxValue = isExpert ? expertMaxValue : masterMaxValue;
	int32_t skillId = 0;
	int32_t skillLevel = 0;
	int32_t maxCraftStatus =
		isExpert ? configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.load() : configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.load();
	int32_t countCraftStatus = 0;
	for (const runtime::Ptr<model::skill::PlayerSkillEntry>& skill : player.getSkillList()->getAllSkills()) {
		countCraftStatus = isExpert ? CraftSkillUpdateService::getInstance().getTotalMasterCraftingSkills(player) +
				CraftSkillUpdateService::getInstance().getTotalExpertCraftingSkills(player)
									: CraftSkillUpdateService::getInstance().getTotalMasterCraftingSkills(player);
		if (countCraftStatus > maxCraftStatus) {
			skillId = skill->getSkillId();
			skillLevel = skill->getSkillLevel();
			if (skill->isCraftingSkill() && skillLevel > minValue && skillLevel <= maxValue) {
				skill->setSkillLvl(minValue - 1);
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SKILL_LIST(*skill, skillMessageId));
				removeRecipesAbove(player, skillId, minValue);
				deleteCraftStatusQuests(skillId, player, isExpert);
			}
			continue;
		}
		break;
	}
	if (!isExpert) {
		removeExcessCraftStatus(player, true);
	}
}

} // namespace aion::gameserver::services::craft
