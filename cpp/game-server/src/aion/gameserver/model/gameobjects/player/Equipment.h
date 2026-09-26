#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`const std::unique_ptr<Equipment>`, parts.json), bound to the
 * player in the constructor. Java's `Collections.synchronizedSortedMap(new TreeMap<>())` is the TreeMap shim (its Monitor is the synchronized
 * map's mutex, so `synchronized (equipment)` blocks become `SYNCHRONIZED(equipment)`). The anonymous RequestResponseHandler, ItemUseObserver and
 * Runnable of soulBindItem are the fieldmap callback structs Equipment_RequestResponseHandler, Equipment_ItemUseObserver and Equipment_Runnable,
 * defined in Equipment.cpp when soulBindItem is ported (§7.3).
 *
 * @author Avol, ATracer, kosyachok, cura
 */
class Equipment : public runtime::OwnedPart, public Persistable {
private:
	// Java: private static final Logger log = LoggerFactory.getLogger(Equipment.class) - namespace-scope logger in Equipment.cpp
	runtime::TreeMap<int64_t, runtime::Ref<Item>> equipment{AION_LOCK_CLASS(Equipment::equipment)};
	runtime::OwnerRef<Player> owner;
	runtime::Field<PersistentState> persistentState{PersistentState::UPDATED};

public:
	explicit Equipment(Player& player);

	~Equipment() override;

	runtime::Ptr<Item> equipItem(int32_t itemUniqueId, int64_t slot);

private:
	bool checkInventorySlots(int64_t itemSlotToEquip);

	bool checkDualWieldRestriction(Item& item, int64_t slot);

	runtime::Ptr<Item> equip(int64_t itemSlotToEquip, Item& item);

	int64_t getUnequipSlots(int64_t itemSlotToEquip);

	void notifyItemEquipped(Item& item);

	void notifyItemUnequip(Item& item);

	void tryUpdateSummonStats();

public:
	/** Called when CM_EQUIP_ITEM packet arrives with action 1 */
	runtime::Ptr<Item> unEquipItem(int32_t itemObjId, bool checkFullInventory);

	runtime::Ptr<Item> unEquipItem(int32_t itemObjId);

private:
	void unEquip(int64_t slot);

	void unequip(Item& item);

	bool checkAvailableEquipSkills(Item& item);

public:
	runtime::Ptr<Item> getEquippedItemByObjId(int32_t itemObjId);

	std::vector<runtime::Ptr<Item>> getEquippedItemsByItemId(int32_t value);

	std::vector<runtime::Ptr<Item>> getEquippedItems();

	std::unordered_set<int32_t> getEquippedItemIds();

	std::vector<runtime::Ptr<Item>> getEquippedItemsWithoutStigma();

	std::vector<runtime::Ptr<Item>> getEquippedForAppearance();

	std::vector<runtime::Ptr<Item>> getEquippedItemsAllStigma();

	std::vector<runtime::Ptr<Item>> getEquippedItemsRegularStigma();

	std::vector<runtime::Ptr<Item>> getEquippedItemsAdvancedStigma();

	int32_t itemSetPartsEquipped(int32_t itemSetTemplateId);

	/** Should be called only when loading from DB for items isEquipped=1 */
	void onLoadHandler(Item& item);

private:
	void putItemBackToInventory(Item& item);

public:
	/**
	 * Should be called only when equipment object totally constructed on player loading. Applies every equipped item stats modificators
	 */
	void onLoadApplyEquipmentStats();

	bool isShieldEquipped();

	runtime::Ptr<Item> getEquippedShield();

	/** @return the main hand weapon type, null without a weapon (StatWeaponMasteryFunction.java:31 tests it) */
	std::optional<templates::item::enums::ItemGroup> getMainHandWeaponType();

	/** @return the off hand weapon type, null without a weapon */
	std::optional<templates::item::enums::ItemGroup> getOffHandWeaponType();

	bool isPowerShardEquipped();

	runtime::Ptr<Item> getMainHandPowerShard();

	runtime::Ptr<Item> getOffHandPowerShard();

	void usePowerShard(Item& powerShardItem, int32_t count);

	/** increase item count and return left count */
	int64_t increaseEquippedItemCount(Item& item, int64_t count);

	void decreaseEquippedItemCount(int32_t itemObjId, int32_t count);

	/** Switch OFF and MAIN hands */
	void switchHands();

	bool isWeaponEquipped(templates::item::enums::ItemSubType subType);

	bool isDualWeaponEquipped();

	/** Only used for new Player creation. Although invalid, but fits its purpose */
	bool isSlotEquipped(int64_t slot);

	runtime::Ptr<Item> getMainHandWeapon();

	runtime::Ptr<Item> getOffHandWeapon();

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState persistentState) override;

private:
	bool soulBindItem(Player& player, Item& item, int64_t slot);

	bool verifyRankLimits(Item& item);

public:
	void checkRankLimitItems();
};

} // namespace aion::gameserver::model::gameobjects::player
