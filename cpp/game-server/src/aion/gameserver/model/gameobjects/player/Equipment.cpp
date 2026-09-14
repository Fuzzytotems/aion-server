#include "aion/gameserver/model/gameobjects/player/Equipment.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::gameobjects::player {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.Equipment");

Equipment::Equipment(Player& player) : OwnedPart(player), owner(player) {
}

Equipment::~Equipment() = default;

runtime::Ptr<Item> Equipment::equipItem(int32_t itemUniqueId, int64_t slot) {
	AION_UNPORTED();
}

bool Equipment::checkInventorySlots(int64_t itemSlotToEquip) {
	AION_UNPORTED();
}

bool Equipment::checkDualWieldRestriction(Item& item, int64_t slot) {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::equip(int64_t itemSlotToEquip, Item& item) {
	AION_UNPORTED();
}

int64_t Equipment::getUnequipSlots(int64_t itemSlotToEquip) {
	AION_UNPORTED();
}

void Equipment::notifyItemEquipped(Item& item) {
	AION_UNPORTED();
}

void Equipment::notifyItemUnequip(Item& item) {
	AION_UNPORTED();
}

void Equipment::tryUpdateSummonStats() {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::unEquipItem(int32_t itemObjId, bool checkFullInventory) {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::unEquipItem(int32_t itemObjId) {
	AION_UNPORTED();
}

void Equipment::unEquip(int64_t slot) {
	AION_UNPORTED();
}

void Equipment::unequip(Item& item) {
	AION_UNPORTED();
}

bool Equipment::checkAvailableEquipSkills(Item& item) {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::getEquippedItemByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsByItemId(int32_t value) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItems() {
	AION_UNPORTED();
}

std::unordered_set<int32_t> Equipment::getEquippedItemIds() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsWithoutStigma() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedForAppearance() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsAllStigma() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsRegularStigma() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsAdvancedStigma() {
	AION_UNPORTED();
}

int32_t Equipment::itemSetPartsEquipped(int32_t itemSetTemplateId) {
	AION_UNPORTED();
}

void Equipment::onLoadHandler(Item& item) {
	AION_UNPORTED();
}

void Equipment::putItemBackToInventory(Item& item) {
	AION_UNPORTED();
}

void Equipment::onLoadApplyEquipmentStats() {
	AION_UNPORTED();
}

bool Equipment::isShieldEquipped() {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::getEquippedShield() {
	AION_UNPORTED();
}

std::optional<templates::item::enums::ItemGroup> Equipment::getMainHandWeaponType() {
	AION_UNPORTED();
}

std::optional<templates::item::enums::ItemGroup> Equipment::getOffHandWeaponType() {
	AION_UNPORTED();
}

bool Equipment::isPowerShardEquipped() {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::getMainHandPowerShard() {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::getOffHandPowerShard() {
	AION_UNPORTED();
}

void Equipment::usePowerShard(Item& powerShardItem, int32_t value) {
	AION_UNPORTED();
}

int64_t Equipment::increaseEquippedItemCount(Item& item, int64_t value) {
	AION_UNPORTED();
}

void Equipment::decreaseEquippedItemCount(int32_t itemObjId, int32_t value) {
	AION_UNPORTED();
}

void Equipment::switchHands() {
	AION_UNPORTED();
}

bool Equipment::isWeaponEquipped(templates::item::enums::ItemSubType subType) {
	AION_UNPORTED();
}

bool Equipment::isDualWeaponEquipped() {
	AION_UNPORTED();
}

bool Equipment::isSlotEquipped(int64_t slot) {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::getMainHandWeapon() {
	AION_UNPORTED();
}

runtime::Ptr<Item> Equipment::getOffHandWeapon() {
	AION_UNPORTED();
}

void Equipment::setPersistentState(PersistentState value) {
	AION_UNPORTED();
}

// Callback structs (fieldmap.py --class): Equipment$1 (RequestResponseHandler), Equipment$2 (ItemUseObserver), Equipment$3 (Runnable)
bool Equipment::soulBindItem(Player& player, Item& item, int64_t slot) {
	AION_UNPORTED();
}

bool Equipment::verifyRankLimits(Item& item) {
	AION_UNPORTED();
}

void Equipment::checkRankLimitItems() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
