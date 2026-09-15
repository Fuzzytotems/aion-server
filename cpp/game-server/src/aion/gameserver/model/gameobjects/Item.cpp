#include "aion/gameserver/model/gameobjects/Item.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/enchants/TemperingEffect.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/templates/item/GodstoneInfo.h"
#include "aion/gameserver/model/templates/item/Improvement.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/Stigma.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::model::gameobjects {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.Item");

namespace {

/** Java `DataManager.ITEM_DATA.getItemTemplate(itemId)` (NullPointerException while the item data is not published) */
const templates::item::ItemTemplate* findItemTemplate(int32_t itemId) {
	return dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
}

/** Java: Objects.requireNonNull(DataManager.ITEM_DATA.getItemTemplate(itemId), () -> "Missing template for item " + itemId) */
const templates::item::ItemTemplate* requireItemTemplate(int32_t itemId) {
	const templates::item::ItemTemplate* itemTemplate = findItemTemplate(itemId);
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("Missing template for item " + std::to_string(itemId));
	return itemTemplate;
}

/** Java dereferences the template in the constructor (NullPointerException for null) */
const templates::item::ItemTemplate* requireTemplate(const templates::item::ItemTemplate* itemTemplate) {
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("itemTemplate");
	return itemTemplate;
}

std::string javaBoolean(bool value) {
	return value ? "true" : "false";
}

/** Java AbstractCollection.toString of a TreeSet<ManaStone> (ManaStone keeps Object.toString: class name and identity hash) */
std::string stoneSetToString(runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones) {
	if (!stones)
		return "null";
	std::string result = "[";
	bool first = true;
	for (runtime::Ptr<items::ManaStone> stone : *stones) {
		if (!first)
			result += ", ";
		first = false;
		result += std::format("com.aionemu.gameserver.model.items.ManaStone@{:x}", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(stone.rawPointer())));
	}
	return result + "]";
}

/** Java: new TreeSet<>(comparator by slot) of itemStonesCollection() */
runtime::Ref<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> newItemStoneSet(const runtime::LockClass& lockClass) {
	return runtime::RcTreeSet<runtime::Ref<items::ManaStone>>::create(lockClass,
		[](const runtime::Ptr<items::ManaStone>& o1, const runtime::Ptr<items::ManaStone>& o2) -> int32_t {
			if (o1->getSlot() == o2->getSlot())
				return 0;
			return o1->getSlot() > o2->getSlot() ? 1 : -1;
		});
}

/**
 * Deviation (D6, docs/deviations/P4-11a.md): Java's lazy `if (field == null) field = new ...` lets two first calls create two collections and
 * lose the elements added to the one that is overwritten. The collection is published with a compare-and-set; a losing caller uses the winner's.
 */
template <class X>
runtime::Ptr<X> publishLazily(runtime::Field<runtime::Ref<X>>& field, runtime::Ref<X> created) {
	runtime::Ptr<X> createdPtr = created;
	return field.compareAndSet(nullptr, std::move(created)) ? createdPtr : field.get();
}

} // namespace

Item::Item(int32_t objId, const templates::item::ItemTemplate* itemTemplateValue)
	: AionObject(objId), itemTemplate(requireTemplate(itemTemplateValue)), equipmentSlot(items::storage::ItemStorage::FIRST_AVAILABLE_SLOT),
	  expireTime(itemTemplateValue->getExpireTime() != 0
			  ? (static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000) + itemTemplateValue->getExpireTime() * 60) - 1
			  : 0) {
	activationCount.set(itemTemplate->getActivationCount());
	if (itemTemplate->canTune())
		tuneCount.set(-1); // not identified yet (bonus stats need to be rolled)
	isAmplified_.set(itemTemplate->getEnchantType() == 1);
	persistentState.set(PersistentState::NEW);
	updateChargeInfo(0);
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
	  itemTemplate(requireItemTemplate(itemId)),
	  isEquipped_(isEquippedValue), equipmentSlot(equipmentSlotValue), optionalSockets(optionalSocketsValue),
	  fusionedItemOptionalSockets(fusionedItemOptionalSocketsValue), isSoulBound_(isSoulBoundValue), itemLocation(itemLocationValue),
	  enchantLevel(enchant), enchantBonus(enchantBonusValue), expireTime(expireTimeValue), activationCount(activationCountValue),
	  tuneCount(tuneCountValue), packCount(packCountValue), tempering(temperingValue), isAmplified_(isAmplifiedValue), buffSkill(buffSkillValue),
	  rndPlumeBonusValue(rndPlumeBonusValueValue) {
	fusionedItemTemplate.set(findItemTemplate(fusionedItem));
	itemSkinTemplate.set(findItemTemplate(itemSkin));
	if (tuneCountValue == -1 && !itemTemplate->canTune()) {
		tuneCount.set(0); // NC made it not tunable
	}
	if (itemTemplate->getStatBonusSetId() != 0 && statBonusId > 0) {
		setBonusStats(statBonusId, false);
	}
	if (const templates::item::ItemTemplate* fused = fusionedItemTemplate.get()) {
		if (fused->getStatBonusSetId() != 0 && fusionedItemStatBonusId > 0) {
			setFusionedItemBonusStats(fusionedItemStatBonusId, false);
		}
		if (!itemTemplate->isCanFuse() || !itemTemplate->isTwoHandWeapon() || !fused->isCanFuse() || !fused->isTwoHandWeapon()) {
			fusionedItemTemplate.set(nullptr);
			fusionedItemOptionalSockets.set(0);
		}
	}
	persistentState.set(PersistentState::NOACTION); // Java leaves the state null; the C++ Field has no null (DAO items need no store)
	updateChargeInfo(charge);
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

void Item::setTempering(int32_t temperingValue) {
	tempering.set(temperingValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void Item::updateChargeInfo(int32_t charge) {
	int32_t chargeLevel = calculateMaxChargeLevel();
	if (!conditioningInfo.get() && chargeLevel > 0)
		conditioningInfo.set(items::ChargeInfo::create(charge, *this));
	// when break fusioned item and second item has conditioned info - set to null
	if (conditioningInfo.get() && chargeLevel == 0)
		conditioningInfo.set(nullptr);
}

std::string Item::getName() {
	return itemTemplate->getName();
}

std::string Item::getItemCreator() {
	return itemCreator.get(); // Java: itemCreator == null ? "" : itemCreator
}

std::string Item::getItemName() {
	return itemTemplate->getName();
}

bool Item::hasOptionalSocket() {
	return optionalSockets.get() != 0;
}

bool Item::hasOptionalFusionSocket() {
	return fusionedItemOptionalSockets.get() != 0;
}

bool Item::isStigmaChargeable() {
	return itemTemplate->getStigma() != nullptr && itemTemplate->getStigma()->isChargeable();
}

const templates::item::ItemTemplate* Item::getItemSkinTemplate() {
	const templates::item::ItemTemplate* skin = itemSkinTemplate.get();
	if (skin == nullptr)
		return itemTemplate;
	return skin;
}

void Item::setItemSkinTemplate(const templates::item::ItemTemplate* newTemplate) {
	itemSkinTemplate.set(newTemplate);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

bool Item::isSkinnedItem() {
	return getItemSkinTemplate() != itemTemplate;
}

void Item::setItemColor(std::optional<int32_t> color) {
	// use bit mask to ensure valid value range (no alpha channel support)
	itemColor.set(color ? std::optional<int32_t>(*color & 0xFFFFFF) : std::nullopt);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int32_t Item::getColorTimeLeft() {
	int32_t expires = colorExpireTime.get();
	if (expires == 0)
		return 0;
	return static_cast<int32_t>(expires - commons::utils::currentTimeMillis() / 1000);
}

void Item::setColorExpireTime(int32_t dyeRemainsUntil) {
	colorExpireTime.set(dyeRemainsUntil);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int64_t Item::getFreeCount() {
	return itemTemplate->getMaxStackCount() - itemCount.get();
}

void Item::setItemCount(int64_t itemCountValue) {
	itemCount.set(itemCountValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int64_t Item::increaseItemCount(int64_t countValue) {
	if (countValue <= 0) {
		return 0;
	}
	int64_t cap = itemTemplate->getMaxStackCount();
	int64_t current = itemCount.get();
	int64_t addCount = current + countValue > cap ? cap - current : countValue;
	if (addCount != 0) {
		itemCount += addCount;
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	}
	return countValue - addCount;
}

int64_t Item::decreaseItemCount(int64_t countValue) {
	if (countValue <= 0) {
		return 0;
	}
	int64_t current = itemCount.get();
	int64_t removeCount = countValue >= current ? current : countValue;
	itemCount -= removeCount;
	if (itemCount.get() == 0 && !itemTemplate->isKinah()) {
		setPersistentState(PersistentState::DELETED);
	} else {
		setPersistentState(PersistentState::UPDATE_REQUIRED);
	}
	return countValue - removeCount;
}

void Item::setEquipped(bool isEquipped) {
	isEquipped_.set(isEquipped);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void Item::setEquipmentSlot(int64_t equipmentSlotValue) {
	equipmentSlot.set(equipmentSlotValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> Item::getItemStones() {
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = manaStones.get();
	if (!stones)
		stones = publishLazily(manaStones, itemStonesCollection()); // Deviation (D6): see publishLazily
	return stones;
}

runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> Item::getFusionStones() {
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = fusionStones.get();
	if (!stones)
		stones = publishLazily(fusionStones, newItemStoneSet(AION_LOCK_CLASS(Item::fusionStones))); // Deviation (D6): see publishLazily
	return stones;
}

int32_t Item::getFusionStonesSize() {
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = fusionStones.get();
	if (!stones)
		return 0;
	return stones->size();
}

int32_t Item::getItemStonesSize() {
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = manaStones.get();
	if (!stones)
		return 0;
	return stones->size();
}

runtime::Ref<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> Item::itemStonesCollection() {
	return newItemStoneSet(AION_LOCK_CLASS(Item::manaStones));
}

bool Item::hasManaStones() {
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = manaStones.get();
	return stones && stones->size() > 0;
}

bool Item::hasFusionStones() {
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = fusionStones.get();
	return stones && stones->size() > 0;
}

bool Item::hasIdianStone() {
	return idianStone.get() != nullptr;
}

bool Item::hasGodStone() {
	return static_cast<bool>(godStone.get());
}

int32_t Item::getGodStoneId() {
	runtime::Ptr<items::GodStone> stone = godStone.get();
	return !stone ? 0 : stone->getItemId();
}

void Item::addGodStone(int32_t itemId) {
	addGodStone(itemId, 0);
}

void Item::addGodStone(int32_t itemId, int32_t activatedCount) {
	const templates::item::GodstoneInfo* godstoneInfo = findItemTemplate(itemId)->getGodstoneInfo();
	if (godstoneInfo == nullptr) {
		log.warn("Item " + std::to_string(itemId) + " has no godstone info");
		return;
	}
	if (godStone.get())
		setGodStone(nullptr);
	godStone.set(items::GodStone::create(*this, activatedCount, itemId, godstoneInfo, PersistentState::NEW));
}

void Item::setGodStone(runtime::Ptr<items::GodStone> godStoneValue) {
	if (!godStoneValue) {
		runtime::Ptr<items::GodStone> current = godStone.get();
		current->setPersistentState(PersistentState::DELETED); // Java NullPointerException without a god stone
		dao::ItemStoneListDAO::storeGodStones(*current);
	}
	godStone.set(godStoneValue);
}

void Item::setEnchantLevel(int32_t enchantLevelValue) {
	enchantLevel.set(enchantLevelValue);
	if (enchantLevelValue > 0)
		removeRemainingTuningCountIfPossible();
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void Item::setPersistentState(PersistentState persistentStateValue) {
	// java-race: check-then-act with the DAO save thread (InventoryDAO.store sets UPDATED after writing the rows), so a change made in between
	// is marked saved until the next change
	switch (persistentStateValue) {
		case PersistentState::DELETED:
			if (persistentState.get() == PersistentState::NEW)
				persistentState.set(PersistentState::NOACTION);
			else
				persistentState.set(PersistentState::DELETED);
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (persistentState.get() == PersistentState::NEW)
				break;
			[[fallthrough]];
		default:
			persistentState.set(persistentStateValue);
	}
}

void Item::setItemLocation(int32_t storageType) {
	itemLocation.set(storageType);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

int32_t Item::getItemMask() {
	return itemTemplate->getMask();
}

void Item::setSoulBound(bool isSoulBound) {
	isSoulBound_.set(isSoulBound);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

templates::item::enums::EquipType Item::getEquipmentType() {
	if (itemTemplate->isStigma())
		return templates::item::enums::EquipType::STIGMA;
	return itemTemplate->getEquipmentType();
}

int32_t Item::getItemId() {
	return itemTemplate->getTemplateId();
}

int32_t Item::getL10nId() const {
	return itemTemplate->getL10nId();
}

bool Item::hasFusionedItem() {
	return fusionedItemTemplate.get() != nullptr;
}

int32_t Item::getFusionedItemId() {
	const templates::item::ItemTemplate* fused = fusionedItemTemplate.get();
	return fused != nullptr ? fused->getTemplateId() : 0;
}

void Item::setFusionedItem(runtime::Ptr<Item> fusionedItem) {
	if (!fusionedItem)
		setFusionedItem(nullptr, 0, 0);
	else
		setFusionedItem(fusionedItem->getItemTemplate(), fusionedItem->getBonusStatsId(), fusionedItem->getOptionalSockets());
}

void Item::setFusionedItem(const templates::item::ItemTemplate* template_, int32_t bonusStatsId, int32_t optionalSocketsValue) {
	removeAllFusionStones();
	fusionedItemTemplate.set(template_);
	setFusionedItemBonusStats(bonusStatsId, false);
	setFusionedItemOptionalSockets(optionalSocketsValue);
	updateChargeInfo(0);
	if (template_ != nullptr)
		removeRemainingTuningCountIfPossible();
}

void Item::removeAllFusionStones() {
	if (!hasFusionStones())
		return;
	runtime::Ptr<runtime::RcTreeSet<runtime::Ref<items::ManaStone>>> stones = fusionStones.get();
	std::unordered_set<runtime::Ptr<items::ManaStone>> toStore;
	for (runtime::Ptr<items::ManaStone> ms : *stones) {
		ms->setPersistentState(PersistentState::DELETED);
		toStore.insert(ms);
	}
	dao::ItemStoneListDAO::storeFusionStone(toStore);
	stones->clear();
}

int32_t Item::getSockets(bool isFusionItem) {
	int32_t numSockets;
	if (itemTemplate->isWeapon() || itemTemplate->isArmor()) {
		if (isFusionItem) {
			const templates::item::ItemTemplate* fusedTemp = getFusionedItemTemplate();
			if (fusedTemp == nullptr)
				return 0;
			numSockets = fusedTemp->getManastoneSlots() + getFusionedItemOptionalSockets();
		} else {
			numSockets = getItemTemplate()->getManastoneSlots() + getOptionalSockets();
		}
		return std::min(numSockets, MAX_BASIC_STONES);
	}
	return 0;
}

bool Item::isStorableInWarehouse() {
	return (getItemMask() & detail::item_mask::STORABLE_IN_WH) == detail::item_mask::STORABLE_IN_WH;
}

bool Item::isStorableInAccWarehouse() {
	return (getItemMask() & detail::item_mask::STORABLE_IN_AWH) == detail::item_mask::STORABLE_IN_AWH && !isSoulBound();
}

bool Item::isStorableInLegWarehouse() {
	return (getItemMask() & detail::item_mask::STORABLE_IN_LWH) == detail::item_mask::STORABLE_IN_LWH && !isSoulBound();
}

bool Item::isTradeable() {
	return (getItemMask() & detail::item_mask::TRADEABLE) == detail::item_mask::TRADEABLE && !isSoulBound();
}

bool Item::isLegionTradeable() {
	return (getItemMask() & detail::item_mask::LEGION_TRADEABLE) == detail::item_mask::LEGION_TRADEABLE && !isSoulBound();
}

bool Item::isRemodelable() {
	return (getItemMask() & detail::item_mask::REMODELABLE) == detail::item_mask::REMODELABLE;
}

bool Item::isSellable() {
	return (getItemMask() & detail::item_mask::SELLABLE) == detail::item_mask::SELLABLE;
}

bool Item::canApExtract() {
	return (getItemMask() & detail::item_mask::CAN_AP_EXTRACT) == detail::item_mask::CAN_AP_EXTRACT;
}

bool Item::canSocketGodstone() {
	return (getItemMask() & detail::item_mask::CAN_PROC_ENCHANT) == detail::item_mask::CAN_PROC_ENCHANT;
}

int32_t Item::getTemporaryExchangeTimeRemaining() {
	int32_t exchangeTime = temporaryExchangeTime.get();
	if (exchangeTime == 0)
		return 0;
	return exchangeTime - static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
}

void Item::onExpire(player::Player& player) {
	if (isEquipped())
		player.getEquipment().unEquipItem(getObjectId());

	for (size_t ordinal = 0; ordinal < items::storage::detail::STORAGE_TYPE_DATA.size(); ++ordinal) {
		auto i = static_cast<items::storage::StorageType>(ordinal);
		if (i == items::storage::StorageType::LEGION_WAREHOUSE)
			continue;
		runtime::Ptr<items::storage::Storage> storage = player.getStorage(items::storage::getId(i));

		if (storage && storage->getItemByObjId(getObjectId())) {
			storage->delete_(*this);
			switch (i) {
				case items::storage::StorageType::CUBE:
					utils::PacketSendUtility::sendPacket(player,
						network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_ITEM_BY_TIMEOUT(getL10n()));
					break;
				case items::storage::StorageType::ACCOUNT_WAREHOUSE:
				case items::storage::StorageType::REGULAR_WAREHOUSE:
					utils::PacketSendUtility::sendPacket(player,
						network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_ITEM_BY_TIMEOUT_IN_WAREHOUSE(getL10n()));
					break;
				default:
					break;
			}
		}
	}
}

void Item::onBeforeExpire(player::Player& player, int32_t remainingMinutes) {
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_CASH_ITEM_TIME_LEFT(getL10n(), remainingMinutes));
}

int32_t Item::getChargePoints() {
	runtime::Ptr<items::ChargeInfo> info = conditioningInfo.get();
	return info ? info->getChargePoints() : 0;
}

int32_t Item::getChargeLevel() {
	if (getChargePoints() == 0)
		return 0;
	return getChargePoints() > items::ChargeInfo::LEVEL1 ? 2 : 1;
}

int32_t Item::calculateMaxChargeLevel() {
	int32_t chargeLevel = 0;
	if (getImprovement() != nullptr)
		chargeLevel = getImprovement()->getLevel();

	int32_t fusionedChargeLevel = 0;
	if (hasFusionedItem() && fusionedItemTemplate.get()->getImprovement() != nullptr)
		fusionedChargeLevel = fusionedItemTemplate.get()->getImprovement()->getLevel();
	return std::max(chargeLevel, fusionedChargeLevel);
}

int32_t Item::calculateAvailableChargeLevel(player::Player& player) {
	int32_t maxAvailableChargeLevel = calculateMaxChargeLevel();
	const templates::item::ItemTemplate* fused = fusionedItemTemplate.get();
	const templates::item::ItemUseLimits* limits =
		hasFusionedItem() && fused->getLevel() > itemTemplate->getLevel() ? fused->getUseLimits() : itemTemplate->getUseLimits();
	if (limits->getRecommendRank() > 0) {
		int32_t rankLevelDiff = std::max(0, limits->getRecommendRank() - detail::abyssRankId(player.getAbyssRank()->getRank()));
		maxAvailableChargeLevel -= rankLevelDiff;
	}
	return std::max(0, maxAvailableChargeLevel);
}

const templates::item::Improvement* Item::getImprovement() {
	if (itemTemplate->getImprovement() != nullptr)
		return itemTemplate->getImprovement();
	else if (hasFusionedItem() && fusionedItemTemplate.get()->getImprovement() != nullptr)
		return fusionedItemTemplate.get()->getImprovement();
	return nullptr;
}

runtime::Ptr<runtime::RcArrayList<runtime::Ref<stats::calc::functions::StatFunction>>> Item::getCurrentModifiers() {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<stats::calc::functions::StatFunction>>> modifiers = currentModifiers.get();
	if (!modifiers) // Deviation (D6): see publishLazily
		modifiers = publishLazily(currentModifiers,
			runtime::RcArrayList<runtime::Ref<stats::calc::functions::StatFunction>>::create(AION_LOCK_CLASS(Item::currentModifiers)));
	return modifiers;
}

void Item::setCurrentModifiers(const std::vector<runtime::Ptr<stats::calc::functions::StatFunction>>& currentModifiersValue) {
	getCurrentModifiers()->clear();
	getCurrentModifiers()->addAll(currentModifiersValue);
}

int32_t Item::getBonusStatsId() {
	runtime::Ptr<items::RandomBonusEffect> effect = bonusStatsEffect.get();
	return !effect ? 0 : effect->getStatBonusId();
}

void Item::setBonusStats(int32_t statBonusId, bool validate) {
	if (validate && isEquipped_.get())
		log.warn(std::to_string(getItemId()) + " was equipped while switching bonus stats from " + std::to_string(getBonusStatsId()) + " to " +
				std::to_string(statBonusId),
			runtime::IllegalStateException(""));
	if (statBonusId == 0)
		bonusStatsEffect.set(nullptr);
	else
		bonusStatsEffect.set(
			items::RandomBonusEffect::create(templates::item::bonuses::StatBonusType::INVENTORY, itemTemplate->getStatBonusSetId(), statBonusId));
}

void Item::setTuneCount(int32_t tuneCountValue) {
	tuneCount.set(tuneCountValue);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void Item::removeRemainingTuningCountIfPossible() {
	if (isIdentified() && itemTemplate->getMaxTuneCount() > 0 && tuneCount.get() != itemTemplate->getMaxTuneCount())
		setTuneCount(itemTemplate->getMaxTuneCount());
}

bool Item::isIdentified() {
	return tuneCount.get() != -1;
}

int32_t Item::getFusionedItemBonusStatsId() {
	runtime::Ptr<items::RandomBonusEffect> effect = fusionedItemBonusStatsEffect.get();
	return !effect ? 0 : effect->getStatBonusId();
}

void Item::setFusionedItemBonusStats(int32_t statBonusId, bool validate) {
	if (validate && isEquipped_.get())
		log.warn(std::to_string(getItemId()) + " was equipped while switching fusioned bonus stats from " + std::to_string(getFusionedItemBonusStatsId()) +
				" to " + std::to_string(statBonusId),
			runtime::IllegalStateException(""));
	if (statBonusId == 0)
		fusionedItemBonusStatsEffect.set(nullptr);
	else
		fusionedItemBonusStatsEffect.set(items::RandomBonusEffect::create(templates::item::bonuses::StatBonusType::INVENTORY,
			fusionedItemTemplate.get()->getStatBonusSetId(), statBonusId));
}

int32_t Item::getMaxEnchantLevel() {
	return getItemTemplate()->getMaxEnchantLevel() + getEnchantBonus();
}

int32_t Item::getItemEnchantParam() {
	if (getItemTemplate()->isWeapon()) {
		if (getEnchantLevel() >= 5 && getEnchantLevel() < 10)
			return 1;
		else if (getEnchantLevel() >= getMaxEnchantLevel() && getEnchantLevel() < 20)
			return 2;
		else if (getEnchantLevel() >= 20)
			return 20;
	} else {
		if (getTempering() >= 5 && getTempering() < 10)
			return 10;
		else if (getTempering() >= 10)
			return 20;
	}
	return getTempering();
}

void Item::setAmplified(bool isAmplified) {
	isAmplified_.set(isAmplified);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

std::string Item::toString() {
	std::string itemColorText = itemColor.get() ? std::to_string(*itemColor.get()) : std::string("null");
	// Java concatenates the raw field, which stays null unless a creator was stored (the empty Field<std::string> reads as null)
	std::string itemCreatorText = itemCreator.get().empty() ? std::string("null") : itemCreator.get();
	return "Item [getItemId()=" + std::to_string(getItemId()) + ", getObjectId()=" + std::to_string(getObjectId()) +
		", itemCount=" + std::to_string(itemCount.get()) + ", itemColor=" + itemColorText + ", colorExpireTime=" + std::to_string(colorExpireTime.get()) +
		", itemCreator=" + itemCreatorText + ", itemSkinId=" + std::to_string(getItemSkinTemplate()->getTemplateId()) +
		", getFusionedItemId()=" + std::to_string(getFusionedItemId()) + ", isEquipped=" + javaBoolean(isEquipped_.get()) +
		", manaStones=" + stoneSetToString(manaStones.get()) + ", fusionStones=" + stoneSetToString(fusionStones.get()) +
		", optionalSockets=" + std::to_string(optionalSockets.get()) + ", fusionedItemOptionalSockets=" + std::to_string(fusionedItemOptionalSockets.get()) +
		", getGodStoneId()=" + std::to_string(getGodStoneId()) + ", isSoulBound=" + javaBoolean(isSoulBound_.get()) +
		", itemLocation=" + std::to_string(itemLocation.get()) + ", enchantLevel=" + std::to_string(enchantLevel.get()) +
		", enchantBonus=" + std::to_string(enchantBonus.get()) + ", expireTime=" + std::to_string(expireTime) +
		", temporaryExchangeTime=" + std::to_string(temporaryExchangeTime.get()) + ", repurchasePrice=" + std::to_string(repurchasePrice.get()) +
		", activationCount=" + std::to_string(activationCount.get()) + ", bonusNumber=" + std::to_string(getBonusStatsId()) +
		", tuneCount=" + std::to_string(tuneCount.get()) + ", packCount=" + std::to_string(packCount.get()) + ", tempering=" + std::to_string(tempering.get()) +
		", isAmplified=" + javaBoolean(isAmplified_.get()) + ", buffSkill=" + std::to_string(buffSkill.get()) +
		", rndPlumeBonusValue=" + std::to_string(rndPlumeBonusValue.get()) + ", getChargePoints()=" + std::to_string(getChargePoints()) + "]";
}

} // namespace aion::gameserver::model::gameobjects
