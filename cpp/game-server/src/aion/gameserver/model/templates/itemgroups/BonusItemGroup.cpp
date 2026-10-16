#include "aion/gameserver/model/templates/itemgroups/BonusItemGroup.h"

#include "aion/gameserver/model/templates/itemgroups/BossGroup.h"
#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.h"
#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.h"
#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.h"
#include "aion/gameserver/model/templates/itemgroups/EventGroup.h"
#include "aion/gameserver/model/templates/itemgroups/FoodGroup.h"
#include "aion/gameserver/model/templates/itemgroups/GatherGroup.h"
#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.h"
#include "aion/gameserver/model/templates/itemgroups/MedalGroup.h"
#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.h"
#include "aion/gameserver/model/templates/itemgroups/OreGroup.h"

namespace aion::gameserver::model::templates::itemgroups {

namespace {
template <class Entry>
std::vector<const ItemRaceEntry*> pointers(const std::vector<Entry>& items) {
	std::vector<const ItemRaceEntry*> entries;
	entries.reserve(items.size());
	for (const Entry& item : items)
		entries.push_back(&item);
	return entries;
}
} // namespace

std::vector<const ItemRaceEntry*> BonusItemGroup::getItems() const {
	switch (groupClass) {
		case GroupClass::CRAFT_ITEM:
			return pointers(static_cast<const CraftItemGroup&>(*this).getItems());
		case GroupClass::CRAFT_RECIPE:
			return pointers(static_cast<const CraftRecipeGroup&>(*this).getItems());
		case GroupClass::EVENT:
			return pointers(static_cast<const EventGroup&>(*this).getItems());
		case GroupClass::MANASTONE:
			return pointers(static_cast<const ManastoneGroup&>(*this).getItems());
		case GroupClass::FOOD:
			return pointers(static_cast<const FoodGroup&>(*this).getItems());
		case GroupClass::MEDICINE:
			return pointers(static_cast<const MedicineGroup&>(*this).getItems());
		case GroupClass::ORE:
			return pointers(static_cast<const OreGroup&>(*this).getItems());
		case GroupClass::GATHER:
			return pointers(static_cast<const GatherGroup&>(*this).getItems());
		case GroupClass::ENCHANT:
			return pointers(static_cast<const EnchantGroup&>(*this).getItems());
		case GroupClass::BOSS:
			return pointers(static_cast<const BossGroup&>(*this).getItems());
		case GroupClass::MEDAL:
			return pointers(static_cast<const MedalGroup&>(*this).getItems());
	}
	return {};
}

} // namespace aion::gameserver::model::templates::itemgroups
