#include "aion/gameserver/services/reward/BonusService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemGroupsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Chance.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/itemgroups/BonusItemGroup.h"
#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.h"
#include "aion/gameserver/model/templates/quest/QuestBonuses.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/rewards/BonusType.h"

namespace aion::gameserver::services::reward {

using model::templates::itemgroups::BonusItemGroup;
using model::templates::itemgroups::ItemRaceEntry;
using model::templates::rewards::BonusType;

// Java BonusService.java:27-37
std::optional<model::templates::quest::QuestItems> BonusService::getQuestBonus(model::gameobjects::player::Player& player, const model::templates::QuestTemplate* questTemplate) {
	if (questTemplate->getBonus() == nullptr)
		return std::nullopt;

	// Java: a null list (no group could be chosen) returns null here, and an empty one returns null below (Chance.selectElement over no
	// element); the C++ vector is empty in both cases, which gives the same answer (docs/deviations/P5-09a.md, M5d stage 1)
	std::vector<const ItemRaceEntry*> itemsOfRandomGroup = getMatchingItemsOfRandomGroup(player, questTemplate);
	if (itemsOfRandomGroup.empty())
		return std::nullopt;

	const ItemRaceEntry* item = model::Chance::selectElement(itemsOfRandomGroup);
	return item == nullptr ? std::nullopt : std::optional<model::templates::quest::QuestItems>(std::in_place, item->getId(), item->getCount());
}

// Java BonusService.java:39-52
std::vector<const ItemRaceEntry*> BonusService::getMatchingItemsOfRandomGroup(model::gameobjects::player::Player& player, const model::templates::QuestTemplate* questTemplate) {
	std::vector<const BonusItemGroup*> remainingGroups = getBonusGroups(questTemplate->getBonus()->getType());
	// Java: null until a group is chosen (the caller's null check); the empty vector stands for it (see getQuestBonus)
	std::vector<const ItemRaceEntry*> allRewards;

	while (!remainingGroups.empty()) {
		const BonusItemGroup* group = model::Chance::selectElement(remainingGroups, true);
		if (group == nullptr)
			break;
		allRewards.clear();
		for (const ItemRaceEntry* i : group->getItems()) {
			if (i->matches(player.getRace(), *questTemplate))
				allRewards.push_back(i);
		}
		if (!allRewards.empty())
			break;
	}
	return allRewards;
}

// Java BonusService.java:54-81
std::vector<const BonusItemGroup*> BonusService::getBonusGroups(BonusType type) {
	switch (type) {
		// case GATHER:
		// return DataManager.ITEM_GROUPS_DATA.getGatherGroups();
		// case BOSS:
		// return DataManager.ITEM_GROUPS_DATA.getBossGroups();
		// case ENCHANT:
		// return DataManager.ITEM_GROUPS_DATA.getEnchantGroups();
		case BonusType::EVENTS:
			return dataholders::DataManager::ITEM_GROUPS_DATA->getEventGroups();
		case BonusType::FOOD:
			return dataholders::DataManager::ITEM_GROUPS_DATA->getFoodGroups();
		case BonusType::MANASTONE:
			return dataholders::DataManager::ITEM_GROUPS_DATA->getManastoneGroups();
		case BonusType::MEDICINE:
			return dataholders::DataManager::ITEM_GROUPS_DATA->getMedicineGroups();
		case BonusType::MEDAL:
			return dataholders::DataManager::ITEM_GROUPS_DATA->getMedalGroups();
		case BonusType::TASK:
			return dataholders::DataManager::ITEM_GROUPS_DATA->getCraftGroups();
		case BonusType::MOVIE:
		case BonusType::NONE:
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.reward.BonusService")
				.warn("Bonus of type " + std::string(xml::enumName(type)) + " is not implemented");
	}
	return {};
}

} // namespace aion::gameserver::services::reward
