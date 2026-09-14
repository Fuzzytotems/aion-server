#include "aion/gameserver/model/gameobjects/Item.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

// Member types (docs/design/hub-headers.md §3.3): the constructors, the destructor, the part accessor and the Field<Ref> setters need complete
// types whose headers are not S0b hubs (ManaStone, GodStone, IdianStone, ChargeInfo, RandomBonusEffect, PendingTuneResult: P4-13; EnchantEffect,
// TemperingEffect: P5-07; ItemStorage: P4-13). Not an S0b transition guard: the chunk that adds the last of them removes it.
#if __has_include("aion/gameserver/model/items/ManaStone.h") && __has_include("aion/gameserver/model/items/GodStone.h") && \
	__has_include("aion/gameserver/model/items/IdianStone.h") && __has_include("aion/gameserver/model/items/ChargeInfo.h") && \
	__has_include("aion/gameserver/model/items/RandomBonusEffect.h") && __has_include("aion/gameserver/model/items/PendingTuneResult.h") && \
	__has_include("aion/gameserver/model/enchants/EnchantEffect.h") && __has_include("aion/gameserver/model/enchants/TemperingEffect.h") && \
	__has_include("aion/gameserver/model/items/storage/ItemStorage.h")
#define AION_ITEM_MEMBER_TYPES 1
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/enchants/TemperingEffect.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#else
#define AION_ITEM_MEMBER_TYPES 0
#endif

namespace aion::gameserver::model::gameobjects {

[[maybe_unused]] static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.Item");

#if AION_ITEM_MEMBER_TYPES
Item::Item(int32_t objId, const templates::item::ItemTemplate* itemTemplateValue)
	: AionObject(objId), itemTemplate(itemTemplateValue), equipmentSlot(items::storage::ItemStorage::FIRST_AVAILABLE_SLOT), expireTime(0) {
	// Java: activationCount, expireTime (from the template), tuneCount, isAmplified, persistentState = NEW, updateChargeInfo(0)
	AION_UNPORTED();
}

Item::Item(int32_t objId, const templates::item::ItemTemplate* itemTemplateValue, int64_t itemCountValue, bool isEquippedValue,
	int64_t equipmentSlotValue)
	: Item(objId, itemTemplateValue) {
	itemCount.set(itemCountValue);
	isEquipped_.set(isEquippedValue);
	equipmentSlot.set(equipmentSlotValue);
}

Item::Item(int32_t objId, int32_t itemId, int64_t itemCountValue, std::optional<int32_t> itemColorValue, int32_t colorExpires,
	std::string_view itemCreatorValue, int32_t expireTimeValue, int32_t activationCountValue, bool isEquippedValue, bool isSoulBoundValue,
	int64_t equipmentSlotValue, int32_t itemLocationValue, int32_t enchant, int32_t enchantBonusValue, int32_t itemSkin, int32_t fusionedItem,
	int32_t optionalSocketsValue, int32_t fusionedItemOptionalSocketsValue, int32_t charge, int32_t tuneCountValue, int32_t statBonusId,
	int32_t fusionedItemStatBonusId, int32_t temperingValue, int32_t packCountValue, bool isAmplifiedValue, int32_t buffSkillValue,
	int32_t rndPlumeBonusValueValue)
	: AionObject(objId), itemCount(itemCountValue), itemColor(itemColorValue), colorExpireTime(colorExpires), itemCreator(std::string(itemCreatorValue)),
	  itemTemplate(nullptr), // Java: Objects.requireNonNull(DataManager.ITEM_DATA.getItemTemplate(itemId), ...)
	  isEquipped_(isEquippedValue), equipmentSlot(equipmentSlotValue), optionalSockets(optionalSocketsValue),
	  fusionedItemOptionalSockets(fusionedItemOptionalSocketsValue), isSoulBound_(isSoulBoundValue), itemLocation(itemLocationValue),
	  enchantLevel(enchant), enchantBonus(enchantBonusValue), expireTime(expireTimeValue), activationCount(activationCountValue),
	  tuneCount(tuneCountValue), packCount(packCountValue), tempering(temperingValue), isAmplified_(isAmplifiedValue), buffSkill(buffSkillValue),
	  rndPlumeBonusValue(rndPlumeBonusValueValue) {
	// Java: templates of itemId, itemSkin and fusionedItem from DataManager.ITEM_DATA, the tune count fix, bonus stats, fusion checks,
	// updateChargeInfo(charge)
	static_cast<void>(itemId);
	static_cast<void>(itemSkin);
	static_cast<void>(fusionedItem);
	static_cast<void>(charge);
	static_cast<void>(statBonusId);
	static_cast<void>(fusionedItemStatBonusId);
	AION_UNPORTED();
}

Item::~Item() = default;

runtime::Ref<Item> Item::create(int32_t objId, const templates::item::ItemTemplate* itemTemplateValue) {
	return runtime::makeRef<Item>(objId, itemTemplateValue);
}

runtime::Ref<Item> Item::create(int32_t objId, const templates::item::ItemTemplate* itemTemplateValue, int64_t itemCountValue, bool isEquippedValue,
	int64_t equipmentSlotValue) {
	return runtime::makeRef<Item>(objId, itemTemplateValue, itemCountValue, isEquippedValue, equipmentSlotValue);
}

runtime::Ref<Item> Item::create(int32_t objId, int32_t itemId, int64_t itemCountValue, std::optional<int32_t> itemColorValue, int32_t colorExpires,
	std::string_view itemCreatorValue, int32_t expireTimeValue, int32_t activationCountValue, bool isEquippedValue, bool isSoulBoundValue,
	int64_t equipmentSlotValue, int32_t itemLocationValue, int32_t enchant, int32_t enchantBonusValue, int32_t itemSkin, int32_t fusionedItem,
	int32_t optionalSocketsValue, int32_t fusionedItemOptionalSocketsValue, int32_t charge, int32_t tuneCountValue, int32_t statBonusId,
	int32_t fusionedItemStatBonusId, int32_t temperingValue, int32_t packCountValue, bool isAmplifiedValue, int32_t buffSkillValue,
	int32_t rndPlumeBonusValueValue) {
	return runtime::makeRef<Item>(objId, itemId, itemCountValue, itemColorValue, colorExpires, itemCreatorValue, expireTimeValue,
		activationCountValue,
		isEquippedValue, isSoulBoundValue, equipmentSlotValue, itemLocationValue, enchant, enchantBonusValue, itemSkin, fusionedItem, optionalSocketsValue,
		fusionedItemOptionalSocketsValue, charge, tuneCountValue, statBonusId, fusionedItemStatBonusId, temperingValue, packCountValue, isAmplifiedValue,
		buffSkillValue, rndPlumeBonusValueValue);
}

runtime::Ptr<items::ChargeInfo> Item::getConditioningInfo() const {
	return runtime::Ptr<items::ChargeInfo>(conditioningInfo.get());
}

runtime::Ptr<items::IdianStone> Item::getIdianStone() const {
	return runtime::Ptr<items::IdianStone>(idianStone.get());
}

void Item::setIdianStone(std::unique_ptr<items::IdianStone> value) {
	idianStone.set(std::move(value));
}

void Item::setTemperingEffect(runtime::Ptr<enchants::TemperingEffect> value) {
	temperingEffect.set(value);
}

void Item::setEnchantEffect(runtime::Ptr<enchants::EnchantEffect> value) {
	enchantEffect.set(value);
}

void Item::setPendingTuneResult(runtime::Ptr<items::PendingTuneResult> value) {
	pendingTuneResult.set(value);
}
#endif

void Item::setTempering(int32_t temperingValue) {
	AION_UNPORTED();
}

void Item::updateChargeInfo(int32_t charge) {
	AION_UNPORTED();
}

std::string Item::getName() {
	AION_UNPORTED();
}

std::string Item::getItemCreator() {
	AION_UNPORTED();
}

std::string Item::getItemName() {
	AION_UNPORTED();
}

bool Item::hasOptionalSocket() {
	AION_UNPORTED();
}

bool Item::hasOptionalFusionSocket() {
	AION_UNPORTED();
}

bool Item::isStigmaChargeable() {
	AION_UNPORTED();
}

const templates::item::ItemTemplate* Item::getItemSkinTemplate() {
	AION_UNPORTED();
}

void Item::setItemSkinTemplate(const templates::item::ItemTemplate* newTemplate) {
	AION_UNPORTED();
}

bool Item::isSkinnedItem() {
	AION_UNPORTED();
}

void Item::setItemColor(std::optional<int32_t> color) {
	AION_UNPORTED();
}

int32_t Item::getColorTimeLeft() {
	AION_UNPORTED();
}

void Item::setColorExpireTime(int32_t dyeRemainsUntil) {
	AION_UNPORTED();
}

int64_t Item::getFreeCount() {
	AION_UNPORTED();
}

void Item::setItemCount(int64_t itemCountValue) {
	AION_UNPORTED();
}

int64_t Item::increaseItemCount(int64_t countValue) {
	AION_UNPORTED();
}

int64_t Item::decreaseItemCount(int64_t countValue) {
	AION_UNPORTED();
}

void Item::setEquipped(bool isEquipped) {
	AION_UNPORTED();
}

void Item::setEquipmentSlot(int64_t equipmentSlotValue) {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> Item::getItemStones() {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> Item::getFusionStones() {
	AION_UNPORTED();
}

int32_t Item::getFusionStonesSize() {
	AION_UNPORTED();
}

int32_t Item::getItemStonesSize() {
	AION_UNPORTED();
}

runtime::Ref<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> Item::itemStonesCollection() {
	AION_UNPORTED();
}

bool Item::hasManaStones() {
	AION_UNPORTED();
}

bool Item::hasFusionStones() {
	AION_UNPORTED();
}

bool Item::hasIdianStone() {
	AION_UNPORTED();
}

bool Item::hasGodStone() {
	AION_UNPORTED();
}

int32_t Item::getGodStoneId() {
	AION_UNPORTED();
}

void Item::addGodStone(int32_t itemId) {
	AION_UNPORTED();
}

void Item::addGodStone(int32_t itemId, int32_t activatedCount) {
	AION_UNPORTED();
}

void Item::setGodStone(runtime::Ptr<items::GodStone> godStoneValue) {
	AION_UNPORTED();
}

void Item::setEnchantLevel(int32_t enchantLevelValue) {
	AION_UNPORTED();
}

void Item::setPersistentState(PersistentState persistentStateValue) {
	AION_UNPORTED();
}

void Item::setItemLocation(int32_t storageType) {
	AION_UNPORTED();
}

int32_t Item::getItemMask() {
	AION_UNPORTED();
}

void Item::setSoulBound(bool isSoulBound) {
	AION_UNPORTED();
}

templates::item::enums::EquipType Item::getEquipmentType() {
	AION_UNPORTED();
}

int32_t Item::getItemId() {
	AION_UNPORTED();
}

int32_t Item::getL10nId() const {
	AION_UNPORTED();
}

bool Item::hasFusionedItem() {
	AION_UNPORTED();
}

int32_t Item::getFusionedItemId() {
	AION_UNPORTED();
}

void Item::setFusionedItem(runtime::Ptr<Item> fusionedItem) {
	AION_UNPORTED();
}

void Item::setFusionedItem(const templates::item::ItemTemplate* template_, int32_t bonusStatsId, int32_t optionalSocketsValue) {
	AION_UNPORTED();
}

void Item::removeAllFusionStones() {
	AION_UNPORTED();
}

int32_t Item::getSockets(bool isFusionItem) {
	AION_UNPORTED();
}

bool Item::isStorableInWarehouse() {
	AION_UNPORTED();
}

bool Item::isStorableInAccWarehouse() {
	AION_UNPORTED();
}

bool Item::isStorableInLegWarehouse() {
	AION_UNPORTED();
}

bool Item::isTradeable() {
	AION_UNPORTED();
}

bool Item::isLegionTradeable() {
	AION_UNPORTED();
}

bool Item::isRemodelable() {
	AION_UNPORTED();
}

bool Item::isSellable() {
	AION_UNPORTED();
}

bool Item::canApExtract() {
	AION_UNPORTED();
}

bool Item::canSocketGodstone() {
	AION_UNPORTED();
}

int32_t Item::getTemporaryExchangeTimeRemaining() {
	AION_UNPORTED();
}

void Item::onExpire(player::Player& player) {
	AION_UNPORTED();
}

void Item::onBeforeExpire(player::Player& player, int32_t remainingMinutes) {
	AION_UNPORTED();
}

int32_t Item::getChargePoints() {
	AION_UNPORTED();
}

int32_t Item::getChargeLevel() {
	AION_UNPORTED();
}

int32_t Item::calculateMaxChargeLevel() {
	AION_UNPORTED();
}

int32_t Item::calculateAvailableChargeLevel(player::Player& player) {
	AION_UNPORTED();
}

const templates::item::Improvement* Item::getImprovement() {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcArrayList<const stats::calc::functions::StatFunction*>> Item::getCurrentModifiers() {
	AION_UNPORTED();
}

void Item::setCurrentModifiers(const std::vector<const stats::calc::functions::StatFunction*>& currentModifiersValue) {
	AION_UNPORTED();
}

int32_t Item::getBonusStatsId() {
	AION_UNPORTED();
}

void Item::setBonusStats(int32_t statBonusId, bool validate) {
	AION_UNPORTED();
}

void Item::setTuneCount(int32_t tuneCountValue) {
	AION_UNPORTED();
}

void Item::removeRemainingTuningCountIfPossible() {
	AION_UNPORTED();
}

bool Item::isIdentified() {
	AION_UNPORTED();
}

int32_t Item::getFusionedItemBonusStatsId() {
	AION_UNPORTED();
}

void Item::setFusionedItemBonusStats(int32_t statBonusId, bool validate) {
	AION_UNPORTED();
}

int32_t Item::getMaxEnchantLevel() {
	AION_UNPORTED();
}

int32_t Item::getItemEnchantParam() {
	AION_UNPORTED();
}

void Item::setAmplified(bool isAmplified) {
	AION_UNPORTED();
}

std::string Item::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
