#include "aion/gameserver/dataholders/ItemGroupsData.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::templates::itemgroups::BonusItemGroup;
using model::templates::itemgroups::ItemRaceEntry;
using model::templates::pet::FoodType;

namespace {

/** Java `group.getItems()` on a group field that may be null */
template <class Group>
const Group& deref(const std::unique_ptr<Group>& group, const char* name) {
	if (group == nullptr)
		throw runtime::NullPointerException(std::string("Cannot invoke \"getItems()\" because \"this.") + name + "\" is null");
	return *group;
}

} // namespace

void ItemGroupsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	craftGroups = {craftMaterials.get(), craftShop.get(), craftBundles.get(), craftRecipes.get()};
	manastoneGroups = {manastonesCommon.get(), manastonesRare.get()};
	medalGroups = {medals.get()};
	foodGroups = {food.get()};
	medicineGroups = {medicineCommon.get(), medicineRare.get(), medicineLegendary.get()};
	gatherGroups = {gatherRare.get(), oresRare.get(), oresLegendary.get(), oresUnique.get(), oresEpic.get()};
	enchantGroups = {enchants.get()};
	eventGroups = {events.get()};
	bossGroups = {bossRare.get(), bossLegendary.get()};

	for (size_t ordinal = 0; ordinal < xml::EnumTraits<FoodType>::names.size(); ++ordinal) {
		const FoodType foodType = static_cast<FoodType>(ordinal);
		const std::vector<ItemRaceEntry>* foodEntries = getPetFood(foodType);
		if (foodEntries == nullptr)
			continue;
		std::unordered_set<int32_t> itemIds;
		for (const ItemRaceEntry& entry : *foodEntries)
			itemIds.insert(entry.getId());
		const int32_t count = static_cast<int32_t>(itemIds.size());
		petFood.insert_or_assign(foodType, std::move(itemIds));
		if (foodType != FoodType::EXCLUDES && foodType != FoodType::STINKY)
			petFoodCount += count;
		// Java: food.clear() (the C++ lists stay, see the class comment)
	}
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getCraftGroups() const {
	return craftGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getManastoneGroups() const {
	return manastoneGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getMedalGroups() const {
	return medalGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getFoodGroups() const {
	return foodGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getMedicineGroups() const {
	return medicineGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getGatherGroups() const {
	return gatherGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getEnchantGroups() const {
	return enchantGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getEventGroups() const {
	return eventGroups;
}

const std::vector<const BonusItemGroup*>& ItemGroupsData::getBossGroups() const {
	return bossGroups;
}

const std::unordered_set<int32_t>& ItemGroupsData::petFoodOf(FoodType foodType) const {
	auto it = petFood.find(foodType);
	if (it == petFood.end())
		throw runtime::NullPointerException("Cannot invoke \"java.util.Set.contains(Object)\" because \"food\" is null"); // Java: a missing entry
	return it->second;
}

bool ItemGroupsData::isFood(int32_t itemId, FoodType foodType) const {
	if (petFoodOf(FoodType::EXCLUDES).contains(itemId))
		return false;
	if (petFoodOf(FoodType::STINKY).contains(itemId))
		return false;
	if (foodType != FoodType::MISCELLANEOUS) {
		return petFoodOf(foodType).contains(itemId);
	} else {
		for (FoodType junk : {FoodType::ARMOR, FoodType::BALAUR_SCALES, FoodType::BONES, FoodType::FLUIDS, FoodType::SOULS, FoodType::THORNS}) {
			if (petFoodOf(junk).contains(itemId))
				return true;
		}
	}
	return false;
}

const std::vector<ItemRaceEntry>* ItemGroupsData::getPetFood(FoodType foodType) const {
	switch (foodType) {
		// Biscuits bought from shop
		case FoodType::AETHER_CRYSTAL_BISCUIT:
			return &deref(aetherCrystalBiscuit, "aetherCrystalBiscuit").getItems();
		case FoodType::AETHER_GEM_BISCUIT:
			return &deref(aetherGemBiscuit, "aetherGemBiscuit").getItems();
		case FoodType::AETHER_POWDER_BISCUIT:
			return &deref(aetherPowderBiscuit, "aetherPowderBiscuit").getItems();
		case FoodType::AETHER_CHERRY:
			return &deref(aetherCherries, "aetherCherries").getItems();

		// Specific Junk
		case FoodType::ARMOR:
			return &deref(feedArmor, "feedArmor").getItems();
		case FoodType::BALAUR_SCALES:
			return &deref(feedBalaurScales, "feedBalaurScales").getItems();
		case FoodType::BONES:
			return &deref(feedBones, "feedBones").getItems();
		case FoodType::FLUIDS:
			return &deref(feedFluids, "feedFluids").getItems();
		case FoodType::SOULS:
			return &deref(feedSouls, "feedSouls").getItems();
		case FoodType::THORNS:
			return &deref(feedThorns, "feedThorns").getItems();

		// Healthy Pet Food bought from vendors
		case FoodType::HEALTHY_FOOD_ALL:
			return &deref(healthyFoodAll, "healthyFoodAll").getItems();
		case FoodType::HEALTHY_FOOD_SPICY:
			return &deref(healthyFoodSpicy, "healthyFoodSpicy").getItems();

		// Runaway Poppy's Food
		case FoodType::POPPY_SNACK:
			return &deref(poppySnack, "poppySnack").getItems();
		case FoodType::POPPY_SNACK_TASTY:
			return &deref(poppySnackTasty, "poppySnackTasty").getItems();
		case FoodType::POPPY_SNACK_NUTRITIOUS:
			return &deref(poppySnackNutritious, "poppySnackNutritious").getItems();

		// Shugo Tomb Event pet food
		case FoodType::SHUGO_EVENT_COIN:
			return &deref(shugoCoins, "shugoCoins").getItems();

		// Exclusions
		case FoodType::STINKY:
			return &deref(stinkingJunk, "stinkingJunk").getItems();
		case FoodType::EXCLUDES:
			return &deref(feedExcludes, "feedExcludes").getItems();
		case FoodType::MISCELLANEOUS:
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.ItemGroupsData")
			  .warn("Unhandled food type " + std::string(xml::enumName(foodType)));
	}
	return nullptr;
}

int32_t ItemGroupsData::bonusSize() const {
	size_t sum = deref(craftMaterials, "craftMaterials").getItems().size() + deref(craftShop, "craftShop").getItems().size() +
	             deref(craftBundles, "craftBundles").getItems().size() + deref(craftRecipes, "craftRecipes").getItems().size() +
	             deref(manastonesCommon, "manastonesCommon").getItems().size() + deref(manastonesRare, "manastonesRare").getItems().size() +
	             deref(food, "food").getItems().size() + deref(medicineCommon, "medicineCommon").getItems().size() +
	             deref(medicineRare, "medicineRare").getItems().size() + deref(medicineLegendary, "medicineLegendary").getItems().size() +
	             deref(oresRare, "oresRare").getItems().size() + deref(oresLegendary, "oresLegendary").getItems().size() +
	             deref(oresUnique, "oresUnique").getItems().size() + deref(oresEpic, "oresEpic").getItems().size() +
	             deref(gatherRare, "gatherRare").getItems().size() + deref(enchants, "enchants").getItems().size() +
	             deref(events, "events").getItems().size() + deref(bossRare, "bossRare").getItems().size() +
	             deref(bossLegendary, "bossLegendary").getItems().size();
	return static_cast<int32_t>(sum);
}

int32_t ItemGroupsData::petFoodSize() const {
	return petFoodCount;
}

} // namespace aion::gameserver::dataholders
