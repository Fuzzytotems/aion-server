#include "aion/gameserver/model/templates/quest/XMLStartCondition.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::quest {

using questEngine::model::QuestState;
using questEngine::model::QuestStatus;

bool XMLStartCondition::checkFinishedQuests(gameobjects::player::QuestStateList& qsl) const {
	for (const FinishedQuestCond& fqc : finished) {
		int32_t questId = fqc.getQuestId();
		int32_t reward = fqc.getReward();
		runtime::Ptr<QuestState> qs = qsl.getQuestState(questId);
		if (!qs || qs->getStatus() != QuestStatus::COMPLETE || (reward >= 0 && (!qs->getRewardGroup() || reward != *qs->getRewardGroup())))
			return false;
		const QuestTemplate* quest = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
		if (quest != nullptr && quest->isRepeatable()) {
			if (quest->getMaxRepeatCount() != 255 && qs->getCompleteCount() != quest->getMaxRepeatCount())
				return false;
		}
	}
	return true;
}

bool XMLStartCondition::checkUnfinishedQuests(gameobjects::player::QuestStateList& qsl) const {
	if (unfinished) {
		for (int32_t questId : *unfinished) {
			runtime::Ptr<QuestState> qs = qsl.getQuestState(questId);
			if (qs && qs->getStatus() == QuestStatus::COMPLETE)
				return false;
		}
	}
	return true;
}

bool XMLStartCondition::checkNoAcquiredQuests(gameobjects::player::QuestStateList& qsl) const {
	if (noacquired) {
		for (int32_t questId : *noacquired) {
			runtime::Ptr<QuestState> qs = qsl.getQuestState(questId);
			if (qs && (qs->getStatus() == QuestStatus::START || qs->getStatus() == QuestStatus::REWARD))
				return false;
		}
	}
	return true;
}

bool XMLStartCondition::checkAcquiredQuests(gameobjects::player::QuestStateList& qsl) const {
	if (acquired) {
		for (int32_t questId : *acquired) {
			runtime::Ptr<QuestState> qs = qsl.getQuestState(questId);
			if (!qs || qs->getStatus() == QuestStatus::LOCKED)
				return false;
		}
	}
	return true;
}

bool XMLStartCondition::checkEquippedItems(gameobjects::player::Player& player, bool warn) const {
	if (!warn)
		return true;
	if (equipped) {
		for (int32_t itemId : *equipped) {
			if (!player.getEquipment().getEquippedItemIds().contains(itemId)) {
				const item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
				if (itemTemplate == nullptr) // Java: NullPointerException on getL10n()
					throw runtime::NullPointerException("Item template " + std::to_string(itemId) + " does not exist");
				std::string requiredItem = itemTemplate->getL10n();
				utils::PacketSendUtility::sendPacket(player,
					network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_EQUIP_ITEM(requiredItem));
				return false;
			}
		}
	}
	return true;
}

bool XMLStartCondition::isRequiredTitleDisplayed(gameobjects::player::Player& player) const {
	if (requiredTitle != 0 && player.getCommonData()->getTitleId() != requiredTitle)
		return false;
	return true;
}

bool XMLStartCondition::check(gameobjects::player::Player& player, bool warn) const {
	runtime::Ptr<gameobjects::player::QuestStateList> qsl = player.getQuestStateList();
	return checkFinishedQuests(*qsl) && checkUnfinishedQuests(*qsl) && checkAcquiredQuests(*qsl) && checkNoAcquiredQuests(*qsl) &&
	       checkEquippedItems(player, warn) && isRequiredTitleDisplayed(player);
}

} // namespace aion::gameserver::model::templates::quest
