#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/**
 * Java com.aionemu.gameserver.model.templates.itemgroups.ItemRaceEntry.
 * <p>
 * C++: Java's overridable getCount, getChance, matchesQuest and matchesLevel (IdLevelReward, CraftReward, CraftItem, CraftRecipe, FoodItem,
 * MedicineItem, FullRewardItem) would be new virtual functions of frozen shells. The entries are not polymorphic in C++: the C++-only member
 * `entryClass`, set by the constructor of each subclass, names the concrete Java class, and the functions of this class dispatch on it to the
 * subclass's non-virtual function of the same name. Calls through an ItemRaceEntry (BonusItemGroup::getItems) therefore give Java's results, and
 * so do calls on the concrete types. Java implements Chance: `Chance::selectElement` is a template that only needs getChance().
 *
 * @author Rolandas
 */
class ItemRaceEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.xml.inc"
public:
	/** C++ only: the concrete Java class of an entry */
	enum class EntryClass : uint8_t { ITEM_RACE_ENTRY, ID_LEVEL_REWARD, CRAFT_ITEM, CRAFT_RECIPE, FOOD_ITEM, MEDICINE_ITEM, FULL_REWARD_ITEM };

private:
	/** C++ only: the dispatch tag, set once through the protected constructor of the concrete class */
	EntryClass entryClass = EntryClass::ITEM_RACE_ENTRY;

public:
	ItemRaceEntry() = default;

	EntryClass getEntryClass() const { return entryClass; }

	/** Java: 1, overridden by CraftItem, FoodItem, MedicineItem and FullRewardItem */
	int64_t getCount() const;

	/** Java Chance.getChance(): 100, overridden by FullRewardItem */
	float getChance() const;

	/** Java final matches(Race, QuestTemplate) */
	bool matches(Race playerRace, const QuestTemplate& questTemplate) const;

protected:
	/** Java: true, overridden by CraftReward, CraftItem and CraftRecipe */
	bool matchesQuest(const QuestTemplate& questTemplate) const;

	/** Java: the bonus level is 0 or the item level, overridden by IdLevelReward */
	bool matchesLevel(const item::ItemTemplate& itemTemplate, int32_t bonusItemLevel) const;

	/** C++ only: the constructor of the subclasses, naming their concrete class */
	explicit ItemRaceEntry(EntryClass value) noexcept : entryClass(value) {}

private:
	bool matchesRace(const item::ItemTemplate& itemTemplate, Race playerRace) const;
};

} // namespace aion::gameserver::model::templates::itemgroups
