#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/rewards/CraftItem.h"
#include "aion/gameserver/model/templates/rewards/CraftRecipe.h"
#include "aion/gameserver/model/templates/rewards/FoodItem.h"
#include "aion/gameserver/model/templates/rewards/FullRewardItem.h"
#include "aion/gameserver/model/templates/rewards/IdLevelReward.h"
#include "aion/gameserver/model/templates/rewards/MedicineItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::itemgroups {

namespace {
/** Java DataManager.ITEM_DATA.getItemTemplate(id), dereferenced by the caller (NullPointerException for an unknown id) */
const item::ItemTemplate& publishedItemTemplate(int32_t id) {
	const item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(id);
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("Item template " + std::to_string(id) + " does not exist");
	return *itemTemplate;
}
} // namespace

void ItemRaceEntry::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: ItemData itemData = staticData != null ? staticData.itemData : DataManager.ITEM_DATA; itemData.getItemTemplate(id).
	// Deviation: the item templates of this load are found through their XmlIDs, without ItemData::getItemTemplate and the fallback to the
	// published ITEM_DATA (the same templates in a full load; //reload is deferred, D3). The IllegalArgumentExceptions are LoadContext::fail calls
	// with the Java messages (docs/deviations/P4-07b.md)
	const item::ItemTemplate* itemTemplate = ctx.findXmlId<item::ItemTemplate>(std::to_string(id));
	if (itemTemplate == nullptr)
		ctx.fail("BonusItemGroup item ID " + std::to_string(id) + " is invalid");
	if (itemTemplate->getRace() != Race::PC_ALL && race != Race::PC_ALL && itemTemplate->getRace() != race)
		ctx.fail("BonusItemGroup item " + std::to_string(id) + " has invalid race " + std::string(xml::enumName(race)) + ". Item is only for " +
		         std::string(xml::enumName(itemTemplate->getRace())));
}

int64_t ItemRaceEntry::getCount() const {
	switch (entryClass) {
		case EntryClass::CRAFT_ITEM:
			return static_cast<const rewards::CraftItem&>(*this).getCount();
		case EntryClass::FOOD_ITEM:
			return static_cast<const rewards::FoodItem&>(*this).getCount();
		case EntryClass::MEDICINE_ITEM:
			return static_cast<const rewards::MedicineItem&>(*this).getCount();
		case EntryClass::FULL_REWARD_ITEM:
			return static_cast<const rewards::FullRewardItem&>(*this).getCount();
		default:
			return 1;
	}
}

float ItemRaceEntry::getChance() const {
	if (entryClass == EntryClass::FULL_REWARD_ITEM)
		return static_cast<const rewards::FullRewardItem&>(*this).getChance();
	return 100.0f;
}

bool ItemRaceEntry::matches(Race playerRace, const QuestTemplate& questTemplate) const {
	const item::ItemTemplate& itemTemplate = publishedItemTemplate(id);
	if (!matchesRace(itemTemplate, playerRace))
		return false;
	if (questTemplate.getBonus() == nullptr) // Java: NullPointerException on getBonus().getLevel()
		throw runtime::NullPointerException("Quest " + std::to_string(questTemplate.getId()) + " has no bonus");
	if (!matchesLevel(itemTemplate, questTemplate.getBonus()->getLevel()))
		return false;
	if (!matchesQuest(questTemplate))
		return false;
	return true;
}

bool ItemRaceEntry::matchesQuest(const QuestTemplate& questTemplate) const {
	switch (entryClass) {
		case EntryClass::CRAFT_ITEM:
			return static_cast<const rewards::CraftItem&>(*this).matchesQuest(questTemplate);
		case EntryClass::CRAFT_RECIPE:
			return static_cast<const rewards::CraftRecipe&>(*this).matchesQuest(questTemplate);
		default:
			return true;
	}
}

bool ItemRaceEntry::matchesLevel(const item::ItemTemplate& itemTemplate, int32_t bonusItemLevel) const {
	switch (entryClass) {
		case EntryClass::ID_LEVEL_REWARD:
		case EntryClass::FOOD_ITEM:
		case EntryClass::MEDICINE_ITEM:
		case EntryClass::FULL_REWARD_ITEM:
			return static_cast<const rewards::IdLevelReward&>(*this).matchesLevel(itemTemplate, bonusItemLevel);
		default:
			return bonusItemLevel == 0 || bonusItemLevel == itemTemplate.getLevel();
	}
}

bool ItemRaceEntry::matchesRace(const item::ItemTemplate& itemTemplate, Race playerRace) const {
	if (itemTemplate.getRace() != Race::PC_ALL && itemTemplate.getRace() != playerRace)
		return false;
	if (race != Race::PC_ALL && race != playerRace)
		return false;
	return true;
}

} // namespace aion::gameserver::model::templates::itemgroups
