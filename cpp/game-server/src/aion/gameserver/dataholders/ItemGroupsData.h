#pragma once

#include <cstdint>
#include <map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/ItemGroupsData.xml.h"
#include "aion/gameserver/model/templates/pet/FoodType.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemGroupsData.
 * <p>
 * C++: the bonus group lists point to the bound groups (an absent group element is nullptr, Java null). afterUnmarshal copies the item ids of
 * the pet food groups into petFood like Java; Java then clears the food lists to save memory, the C++ lists stay (they are private to the holder
 * and unobservable).
 *
 * @author Rolandas
 */
class ItemGroupsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemGroupsData.xml.inc"
private:
	std::vector<const model::templates::itemgroups::BonusItemGroup*> craftGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> manastoneGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> medalGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> foodGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> medicineGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> gatherGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> enchantGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> eventGroups;
	std::vector<const model::templates::itemgroups::BonusItemGroup*> bossGroups;

	/** Java: EnumMap<FoodType, Set<Integer>> */
	std::map<model::templates::pet::FoodType, std::unordered_set<int32_t>> petFood;

	int32_t petFoodCount = 0;

	/** Java petFood.get(foodType), dereferenced: @throws runtime::NullPointerException for a food type without an entry */
	const std::unordered_set<int32_t>& petFoodOf(model::templates::pet::FoodType foodType) const;

public:
	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getCraftGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getManastoneGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getMedalGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getFoodGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getMedicineGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getGatherGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getEnchantGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getEventGroups() const;

	const std::vector<const model::templates::itemgroups::BonusItemGroup*>& getBossGroups() const;

	bool isFood(int32_t itemId, model::templates::pet::FoodType foodType) const;

private:
	/** @return the food entries of the type, nullptr (Java null) for MISCELLANEOUS. @throws NullPointerException if the group element is absent */
	const std::vector<model::templates::itemgroups::ItemRaceEntry>* getPetFood(model::templates::pet::FoodType foodType) const;

public:
	int32_t bonusSize() const;

	int32_t petFoodSize() const;
};

} // namespace aion::gameserver::dataholders
