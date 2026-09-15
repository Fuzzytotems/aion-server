#include "aion/gameserver/model/templates/item/ItemTemplate.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/item/ItemActivationTargetInfo.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"

namespace aion::gameserver::model::templates::item {

namespace {

/**
 * Java com.aionemu.gameserver.model.items.ItemMask and ItemId.KINAH (P4-13, model.items): their C++ headers do not exist yet, so the constants
 * this class reads are repeated here. Values verbatim from ItemMask.java / ItemId.java.
 */
constexpr int32_t ITEM_MASK_LIMIT_ONE = 1;
constexpr int32_t ITEM_MASK_TRADEABLE = 1 << 1;
constexpr int32_t ITEM_MASK_BREAKABLE = 1 << 6;
constexpr int32_t ITEM_MASK_SOUL_BOUND = 1 << 7;
constexpr int32_t ITEM_MASK_NO_ENCHANT = 1 << 9;
constexpr int32_t ITEM_MASK_CAN_COMPOSITE_WEAPON = 1 << 11;
constexpr int32_t ITEM_MASK_CAN_SPLIT = 1 << 13;
constexpr int32_t ITEM_MASK_DELETABLE = 1 << 14;
constexpr int32_t ITEM_MASK_DYEABLE = 1 << 15;
constexpr int32_t ITEM_MASK_CAN_POLISH = 1 << 17;
constexpr int32_t ITEM_ID_KINAH = 182400001;

/** Java `array[playerClass.ordinal()]` with Java's ArrayIndexOutOfBoundsException */
int8_t restrictionAt(const std::vector<int8_t>& restrictions, PlayerClass playerClass) {
	size_t index = static_cast<size_t>(playerClass);
	if (index >= restrictions.size())
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " +
			std::to_string(restrictions.size()));
	return restrictions[index];
}

} // namespace

void ItemTemplate::setXmlUid(std::string_view uid) {
	itemId = commons::utils::parseInt(uid);
}

void ItemTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Deviation: Java assigns the shared static emptyWeaponStats/emptyUseLimits; the generated members own their objects, so each template gets
	// its own default-constructed object (equal values, distinct identity; docs/deviations/P4-07a.md)
	if (weaponStats == nullptr)
		weaponStats = std::make_unique<WeaponStats>(); // Java: emptyWeaponStats
	if (useLimits == nullptr)
		useLimits = std::make_unique<ItemUseLimits>(); // Java: emptyUseLimits

	// check if it can be randomized
	if (getItemSlot() == 0)
		maxTuneCount = 0;
	else if (maxTuneCount == -1) {
		if (maxEnchantBonus == 0 && optionSlotBonus == 0 && rndBonusId == 0)
			maxTuneCount = 0;
	}
}

int64_t ItemTemplate::getItemSlot() const {
	return enums::getValidEquipmentSlots(itemGroup);
}

bool ItemTemplate::isClassSpecific(PlayerClass playerClass) const {
	bool related = restrictionAt(levelRestrictions, playerClass) > 0;
	if (!related && !isStartingClass(playerClass)) {
		related = restrictionAt(levelRestrictions, getStartingClass(playerClass)) > 0;
	}
	return related;
}

int32_t ItemTemplate::getRequiredLevel(PlayerClass playerClass) const {
	int32_t requiredLevel = restrictionAt(levelRestrictions, playerClass);
	if (requiredLevel == 0)
		return -1;
	else
		return requiredLevel;
}

int8_t ItemTemplate::getMaxLevelRestrict(PlayerClass playerClass) const {
	if (maxLevelRestrictions.has_value()) {
		return restrictionAt(*maxLevelRestrictions, playerClass);
	}
	return 0;
}

const std::vector<std::unique_ptr<model::stats::calc::functions::StatFunction>>* ItemTemplate::getModifiers() const {
	if (modifiers != nullptr) {
		return &modifiers->getModifiers();
	}
	return nullptr;
}

enums::ItemSubType ItemTemplate::getItemSubType() const {
	return enums::getItemSubType(itemGroup);
}

enums::EquipType ItemTemplate::getEquipmentType() const {
	return enums::getEquipType(itemGroup);
}

int64_t ItemTemplate::getMaxStackCount() const {
	if (isKinah()) {
		if (configs::main::CustomConfig::ENABLE_KINAH_CAP.load(std::memory_order_relaxed)) {
			return configs::main::CustomConfig::KINAH_CAP_VALUE.load(std::memory_order_relaxed);
		} else {
			return std::numeric_limits<int64_t>::max();
		}
	}
	return maxStackCount;
}

bool ItemTemplate::isNoEnchant() const {
	return (getMask() & ITEM_MASK_NO_ENCHANT) == ITEM_MASK_NO_ENCHANT;
}

bool ItemTemplate::isItemDyePermitted() const {
	return (getMask() & ITEM_MASK_DYEABLE) == ITEM_MASK_DYEABLE;
}

bool ItemTemplate::isWeapon() const {
	return getEquipmentType() == enums::EquipType::WEAPON;
}

bool ItemTemplate::isArmor() const {
	return getEquipmentType() == enums::EquipType::ARMOR;
}

bool ItemTemplate::isKinah() const {
	return itemId == ITEM_ID_KINAH;
}

const itemset::ItemSetTemplate* ItemTemplate::getItemSet() const {
	// Java: DataManager.ITEM_SET_DATA.getItemSetTemplateByItemId(itemId); ItemSetData (P4-09) declares no such method yet
	AION_UNPORTED();
}

bool ItemTemplate::isItemSet() const {
	return getItemSet() != nullptr;
}

bool ItemTemplate::hasLimitOne() const {
	return (getMask() & ITEM_MASK_LIMIT_ONE) == ITEM_MASK_LIMIT_ONE;
}

bool ItemTemplate::isTradeable() const {
	return (getMask() & ITEM_MASK_TRADEABLE) == ITEM_MASK_TRADEABLE;
}

bool ItemTemplate::isCanFuse() const {
	return (getMask() & ITEM_MASK_CAN_COMPOSITE_WEAPON) == ITEM_MASK_CAN_COMPOSITE_WEAPON;
}

bool ItemTemplate::canSplit() const {
	return (getMask() & ITEM_MASK_CAN_SPLIT) == ITEM_MASK_CAN_SPLIT;
}

bool ItemTemplate::isSoulBound() const {
	return (getMask() & ITEM_MASK_SOUL_BOUND) == ITEM_MASK_SOUL_BOUND;
}

bool ItemTemplate::isBreakable() const {
	return (getMask() & ITEM_MASK_BREAKABLE) == ITEM_MASK_BREAKABLE;
}

bool ItemTemplate::isDeletable() const {
	return (getMask() & ITEM_MASK_DELETABLE) == ITEM_MASK_DELETABLE;
}

bool ItemTemplate::isCanPolish() const {
	return (getMask() & ITEM_MASK_CAN_POLISH) == ITEM_MASK_CAN_POLISH;
}

bool ItemTemplate::isTwoHandWeapon() const {
	if (!isWeapon())
		return false;
	return getItemSubType() == enums::ItemSubType::TWO_HAND;
}

bool ItemTemplate::isOneHandWeapon() const {
	if (!isWeapon())
		return false;
	return getItemSubType() == enums::ItemSubType::ONE_HAND;
}

int32_t ItemTemplate::getExtraInventoryId() const {
	if (extraInventory == nullptr) {
		return -1;
	}
	return extraInventory->getId();
}

void ItemTemplate::modifyMask(bool apply, int32_t filter) {
	if (apply)
		mask |= filter;
	else
		mask &= ~filter;
}

bool ItemTemplate::hasAreaRestriction() const {
	return useLimits->getUseArea() != nullptr;
}

const ::aion::gameserver::world::zone::ZoneName* ItemTemplate::getUseArea() const {
	return useLimits->getUseArea();
}

bool ItemTemplate::hasWorldRestrictions() const {
	return !useLimits->getOwnershipWorldIds().empty();
}

bool ItemTemplate::isItemRestrictedToWorld(int32_t worldId) const {
	const std::vector<int32_t>& ownershipWorldIds = useLimits->getOwnershipWorldIds();
	if (ownershipWorldIds.empty())
		return false;
	return std::ranges::find(ownershipWorldIds, worldId) != ownershipWorldIds.end();
}

bool ItemTemplate::isCloth() const {
	// not sure about LT_HEAD and CL_HEAD, check in retail
	return isArmor() && ((enums::getArmorType(itemGroup) != enums::ArmorType::ACCESSORY && getItemGroup() != enums::ItemGroup::BELT) ||
		itemGroup == enums::ItemGroup::HEAD);
}

bool ItemTemplate::isCombinationItem() const {
	return getItemGroup() == enums::ItemGroup::COMBINATION;
}

bool ItemTemplate::isEnchantmentStone() const {
	return getItemGroup() == enums::ItemGroup::ENCHANTMENT;
}

std::optional<ItemActivationTarget> ItemTemplate::getActivationTarget() const {
	if (!getActivationRace().has_value())
		return activationTarget;
	return std::nullopt;
}

std::optional<Race> ItemTemplate::getActivationRace() const {
	return !activationTarget.has_value() ? std::nullopt : item::getRace(*activationTarget); // the companion function (the member getRace() hides ADL)
}

std::span<const int32_t> ItemTemplate::getRequiredSkills() const {
	return enums::getRequiredSkills(itemGroup);
}

} // namespace aion::gameserver::model::templates::item
