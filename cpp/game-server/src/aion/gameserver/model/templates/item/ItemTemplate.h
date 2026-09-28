#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/item/ItemTemplate.xml.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/itemset/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::model::templates::item {

/**
 * Java com.aionemu.gameserver.model.templates.item.ItemTemplate.
 * <p>
 * C++: Java's afterUnmarshal points weaponStats and useLimits at shared empty objects; the generated members own their objects, so the hook
 * gives every template without the element its own default-constructed object (the getters never return null after the hook, as in Java).
 *
 * @author Luno, ATracer
 */
class ItemTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/item/ItemTemplate.xml.inc"
public:
	/** @return the valid equipment slots of the item group (ItemSlot masks) */
	int64_t getItemSlot() const;

	/** @throws ArrayIndexOutOfBoundsException if the restrictions have no entry for the class (Java array access) */
	bool isClassSpecific(PlayerClass playerClass) const;

	/** @return the required level, -1 if the class cannot use the item */
	int32_t getRequiredLevel(PlayerClass playerClass) const;

	int8_t getMaxLevelRestrict(PlayerClass playerClass) const;

	/** @return the stat functions of the modifiers element, nullptr (Java null) without one */
	const std::vector<std::unique_ptr<model::stats::calc::functions::StatFunction>>* getModifiers() const;

	enums::ItemSubType getItemSubType() const;

	enums::EquipType getEquipmentType() const;

	int64_t getPrice() const { return price; }

	int32_t getL10nId() const override { return description; }

	/** @return the max stack count; for kinah the configured cap (CustomConfig.KINAH_CAP_VALUE) or Long.MAX_VALUE */
	int64_t getMaxStackCount() const;

	bool isNoEnchant() const;

	bool isItemDyePermitted() const;

	bool isWeapon() const;

	bool isArmor() const;

	bool isKinah() const;

	bool isStigma() const { return stigma != nullptr; }

	/** @return the associated ItemSetTemplate, nullptr if none */
	const itemset::ItemSetTemplate* getItemSet() const;

	/** Checks if the ItemTemplate belongs to an item set */
	bool isItemSet() const;

	/** @return the name, empty (Java: "" for null) if absent */
	std::string getName() const override { return name; }

	int32_t getTemplateId() const override { return itemId; }

	bool hasLimitOne() const;

	bool isTradeable() const;

	bool isCanFuse() const;

	bool canSplit() const;

	bool isSoulBound() const;

	bool isBreakable() const;

	bool isDeletable() const;

	bool isCanPolish() const;

	bool isTwoHandWeapon() const;

	bool isOneHandWeapon() const;

	/** @return -1 if no id, can be values 0, 1, 2 */
	int32_t getExtraInventoryId() const;

	/** Changes the mask (ItemData.cleanup, before the templates are published) */
	void modifyMask(bool apply, int32_t filter);

	bool isStackable() const { return maxStackCount > 1; }

	bool hasAreaRestriction() const;

	/** @return the use area, nullptr (Java null) if none or if the name is invalid */
	const ::aion::gameserver::world::zone::ZoneName* getUseArea() const;

	bool canTune() const { return maxTuneCount != 0; }

	bool hasWorldRestrictions() const;

	bool isItemRestrictedToWorld(int32_t worldId) const;

	bool isCloth() const;

	bool isPotion() const { return itemId >= 162000000 && itemId < 163000000; }

	bool isCombinationItem() const;

	bool isEnchantmentStone() const;

	/** @return the activation target, std::nullopt (Java null) if there is none or it is a race target */
	std::optional<ItemActivationTarget> getActivationTarget() const;

	/** @return the race of the activation target, std::nullopt (Java null) if there is none */
	std::optional<Race> getActivationRace() const;

	/** @return the robot id, 0 if none */
	int32_t getRobotId() const { return robotId.value_or(0); }

	/** @return the required skills of the item group (Java int[], never null) */
	std::span<const int32_t> getRequiredSkills() const;

private:
	/** Java `private int itemId`: not bound itself, set by the annotated setter setXmlUid (id, the XmlID) */
	int32_t itemId = 0;
};

} // namespace aion::gameserver::model::templates::item
