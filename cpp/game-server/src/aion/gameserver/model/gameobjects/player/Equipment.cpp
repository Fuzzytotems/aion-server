#include "aion/gameserver/model/gameobjects/player/Equipment.h"

#include <algorithm>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/skillengine/effect/WeaponDualEffect.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/StigmaService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::model::gameobjects::player {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.player.Equipment");

namespace {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using state::CreatureState;
using templates::item::enums::ItemGroup;
using templates::item::enums::ItemSubType;
using utils::PacketSendUtility;

/** Java Arrays.toString(ItemSlot[]) */
std::string toString(const std::vector<items::ItemSlot>& slots) {
	std::string text = "[";
	for (size_t i = 0; i < slots.size(); ++i) {
		if (i > 0)
			text += ", ";
		text += xml::EnumTraits<items::ItemSlot>::names[static_cast<size_t>(slots[i])];
	}
	return text + "]";
}

/** Java AuditLogger.log(player, message) */
void auditLog(Player& player, const std::string& message) {
	utils::audit::AuditLogger::log(player, message);
}

/** Java ItemEquipmentListener.onItemEquipment(item, player) */
void onItemEquipment(Item& item, Player& player) {
	stats::listeners::ItemEquipmentListener::onItemEquipment(item, player);
}

/** Java ItemEquipmentListener.onItemUnequipment(item, player) */
void onItemUnequipment(Item& item, Player& player) {
	stats::listeners::ItemEquipmentListener::onItemUnequipment(item, player);
}

/**
 * Java AbyssRankEnum.getRankL10n(race, rankId): getRankById(rankId) (id = ordinal + 1, IllegalArgumentException for an unknown id), then
 * ChatUtil.l10n((race == ELYOS ? 901215 : 901233) + ordinal). TODO(P5-01): call AbyssRankEnum's companion once it exists.
 */
std::string getRankL10n(Race race, int32_t rankId) {
	if (rankId < 1 || rankId > static_cast<int32_t>(utils::stats::AbyssRankEnum::SUPREME_COMMANDER) + 1)
		throw runtime::IllegalArgumentException("Invalid abyss rank provided " + std::to_string(rankId));
	const int32_t rank9L10nId = race == Race::ELYOS ? 901215 : 901233;
	return utils::ChatUtil::l10n(rank9L10nId + rankId - 1);
}

/** Java ActionState.X.getL10n(): ChatUtil.l10n(getL10nId()) */
std::string getL10n(ActionState state) {
	return utils::ChatUtil::l10n(getL10nId(state));
}

/** Java AbyssRankEnum.getId(): ordinal + 1 (GRADE9_SOLDIER 1 .. SUPREME_COMMANDER 18) */
int32_t rankId(utils::stats::AbyssRankEnum rank) {
	return static_cast<int32_t>(rank) + 1;
}

/** True if `items` holds an element Java-equal to `item` (List.contains/HashSet: Item equals compares the object id) */
bool containsEqual(const std::vector<runtime::Ptr<Item>>& items, Item& item) {
	return std::ranges::any_of(items, [&item](const runtime::Ptr<Item>& other) { return other->equals(item); });
}

// fieldmap-class: com.aionemu.gameserver.model.gameobjects.player.Equipment$1
/** Java: the anonymous RequestResponseHandler<Player> of soulBindItem (fieldmap callback struct) */
class Equipment_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<Equipment> equipment; // captured this Equipment
	const runtime::Ref<Item> item;           // captured param Item item
	const int64_t slot;                      // captured param long slot

	static runtime::Ref<Equipment_RequestResponseHandler> create(Player& player, Equipment& equipmentValue, Item& itemValue, int64_t slotValue) {
		return runtime::makeRef<Equipment_RequestResponseHandler>(player, equipmentValue, itemValue, slotValue);
	}

	void acceptRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		// Java: responder.getController().cancelUseItem(); SM_ITEM_USAGE_ANIMATION(..., 5000, 4); attach the anonymous ItemUseObserver
		// (Equipment_ItemUseObserver: cancel ITEM_USE, STR_SOUL_BOUND_ITEM_CANCELED, animation 0/8) and schedule the 5 s Equipment_Runnable
		// (remove the observer, animation 0/6, STR_SOUL_BOUND_ITEM_SUCCEED, item.setSoulBound(true), updateItemAfterInfoChange, equip(slot,
		// item), SM_UPDATE_PLAYER_APPEARANCE). ItemUseObserver (P4-11b) has no C++ header yet.
		static_cast<void>(requesterValue);
		static_cast<void>(responder);
		AION_UNPORTED();
	}

	void denyRequest(runtime::Ptr<Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_ITEM_CANCELED(item->getL10n()));
	}

protected:
	Equipment_RequestResponseHandler(Player& player, Equipment& equipmentValue, Item& itemValue, int64_t slotValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(player)), equipment(equipmentValue), item(itemValue), slot(slotValue) {}
	~Equipment_RequestResponseHandler() override = default;
};

} // namespace

Equipment::Equipment(Player& player) : OwnedPart(player), owner(player) {
}

Equipment::~Equipment() = default;

runtime::Ptr<Item> Equipment::equipItem(int32_t itemUniqueId, int64_t slot) {
	runtime::Ptr<Item> item = owner.getInventory().getItemByObjId(itemUniqueId);
	if (!item || item->isEquipped())
		return nullptr;

	const templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();
	if (itemTemplate->isTwoHandWeapon()) // client only sends main+sub slot when equipping via right click / double click
		slot = detail::MAIN_OR_SUB;
	else if (itemTemplate->isOneHandWeapon() && !skillengine::effect::WeaponDualEffect::hasDualWieldEffect(owner))
		slot = detail::MAIN_HAND;

	if (!itemTemplate->isClassSpecific(owner.getPlayerClass())) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_CLASS());
		return nullptr;
	}
	// don't allow to wear items of not allowed level
	int32_t requiredLevel = itemTemplate->getRequiredLevel(owner.getPlayerClass());
	if (requiredLevel == -1 || requiredLevel > owner.getLevel()) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(item->getL10n(), requiredLevel));
		return nullptr;
	}

	int8_t levelRestrict = itemTemplate->getMaxLevelRestrict(owner.getPlayerClass());
	if (levelRestrict != 0 && owner.getLevel() > levelRestrict) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL(levelRestrict, itemTemplate->getL10n()));
		return nullptr;
	}

	if (itemTemplate->getRace() != Race::PC_ALL && itemTemplate->getRace() != owner.getRace()) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE());
		return nullptr;
	}

	const templates::item::ItemUseLimits* limits = itemTemplate->getUseLimits();
	if (limits->getGenderPermitted() && *limits->getGenderPermitted() != owner.getGender()) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_GENDER());
		return nullptr;
	}

	if (!verifyRankLimits(*item)) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RANK(getRankL10n(owner.getRace(), limits->getMinRank())));
		return nullptr;
	}

	if (!checkInventorySlots(slot)) {
		PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_UI_INVENTORY_FULL());
		return nullptr;
	}

	if (!checkAvailableEquipSkills(*item))
		return nullptr;

	std::vector<items::ItemSlot> targetSlots = detail::getSlotsFor(slot);
	if (targetSlots.empty()) {
		log.warn("Unknown target slot " + std::to_string(slot) + " for " + item->toString());
		return nullptr;
	}

	if ((targetSlots.size() == 2 && !itemTemplate->isTwoHandWeapon()) || targetSlots.size() > 2) {
		auditLog(owner, "tried to equip " + item->toString() + " in slots: " + toString(targetSlots));
		return nullptr;
	}

	if ((detail::MAIN_OFF_OR_SUB_OFF & slot) != 0) { // offhand slots cannot be directly populated on client side
		auditLog(owner, "tried to equip " + item->toString() + " directly in offhand slot");
		return nullptr;
	}

	int64_t validSlotMask = itemTemplate->getItemSlot();
	if (validSlotMask == 0) // e.g. arrows, which cannot be equipped anymore
		return nullptr;
	if ((validSlotMask & slot) != slot) { // invalid slot provided for the item
		auditLog(owner, "tried to equip " + item->toString() + " in invalid slot(s): " + toString(targetSlots));
		return nullptr;
	}

	if (!services::StigmaService::notifyEquipAction(owner, *item, slot))
		return nullptr;

	if (itemTemplate->isSoulBound() && !item->isSoulBound()) {
		soulBindItem(owner, *item, slot);
		return nullptr;
	}
	return equip(slot, *item);
}

bool Equipment::checkInventorySlots(int64_t itemSlotToEquip) {
	if (owner.getInventory().isFull() && detail::isTwoHandedWeapon(itemSlotToEquip)) { // weapon slot(s)
		for (items::ItemSlot slot : detail::getSlotsFor(itemSlotToEquip)) {
			runtime::Ptr<Item> equippedWeaponOrShield = equipment.get(detail::getSlotIdMask(slot));
			if (!equippedWeaponOrShield || equippedWeaponOrShield->getItemTemplate()->isTwoHandWeapon())
				return true;
		}
		return false; // two weapons would need to be unequipped, but there is no free slot
	}
	return true;
}

bool Equipment::checkDualWieldRestriction(Item& item, int64_t slot) {
	if (item.getItemTemplate()->isOneHandWeapon() && (slot & detail::LEFT_HAND) == slot &&
		!skillengine::effect::WeaponDualEffect::hasDualWieldEffect(owner))
		return false;
	return true;
}

runtime::Ptr<Item> Equipment::equip(int64_t itemSlotToEquip, Item& item) {
	if (!item.isIdentified()) {
		log.warn(item.toString() + " can't be equipped because it's not identified yet");
		return nullptr;
	}

	std::vector<items::ItemSlot> targetSlots = detail::getSlotsFor(itemSlotToEquip);

	SYNCHRONIZED(*this) {
		// do unequip of necessary items
		unEquip(getUnequipSlots(itemSlotToEquip));
		owner.getInventory().remove(item);
		// equip target item
		for (items::ItemSlot slot : targetSlots)
			equipment.put(detail::getSlotIdMask(slot), runtime::Ref<Item>(item));
		item.setEquipped(true);
		item.setEquipmentSlot(itemSlotToEquip);
		services::item::ItemPacketService::updateItemAfterEquip(owner, item);

		// update stats
		notifyItemEquipped(item);
		owner.getLifeStats()->updateCurrentStats();
		owner.getGameStats()->updateStatsAndSpeedVisually();
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		questEngine::QuestEngine::getInstance().onEquipItem(*questEngine::model::QuestEnv::create(nullptr, owner, 0), item.getItemId());

		if (item.getItemTemplate()->isStigma())
			services::StigmaService::addLinkedStigmaSkills(owner);

		return runtime::Ptr<Item>(item);
	}
}

int64_t Equipment::getUnequipSlots(int64_t itemSlotToEquip) {
	if (itemSlotToEquip == detail::MAIN_HAND || itemSlotToEquip == detail::SUB_HAND) {
		runtime::Ptr<Item> equippedItem = equipment.get(itemSlotToEquip);
		if (equippedItem && equippedItem->getItemTemplate()->isTwoHandWeapon())
			return detail::MAIN_OR_SUB; // two-handed occupies two slots, so we need to unequip both
	}
	return itemSlotToEquip;
}

void Equipment::notifyItemEquipped(Item& item) {
	onItemEquipment(item, owner);
	owner.getObserveController()->notifyItemEquip(item, owner);
	tryUpdateSummonStats();
}

void Equipment::notifyItemUnequip(Item& item) {
	onItemUnequipment(item, owner);
	owner.getObserveController()->notifyItemUnEquip(item, owner);
	tryUpdateSummonStats();
}

void Equipment::tryUpdateSummonStats() {
	runtime::Ptr<Summon> summon = owner.getSummon();
	if (summon)
		summon->getGameStats()->updateStatsAndSpeedVisually();
}

runtime::Ptr<Item> Equipment::unEquipItem(int32_t itemObjId, bool checkFullInventory) {
	// if inventory is full unequip action is disabled
	if (checkFullInventory && owner.getInventory().isFull())
		return nullptr;

	SYNCHRONIZED(*this) {
		runtime::Ptr<Item> itemToUnequip = getEquippedItemByObjId(itemObjId);
		if (!itemToUnequip || !itemToUnequip->isEquipped())
			return nullptr;

		// Looks very odd - but its retail like
		if (itemToUnequip->getEquipmentSlot() == detail::MAIN_HAND) {
			runtime::Ptr<Item> ohWeapon = equipment.get(detail::SUB_HAND);
			if (ohWeapon && ohWeapon->getItemTemplate()->isWeapon()) {
				if (owner.getInventory().getFreeSlots() < 2)
					return nullptr;
				unEquip(detail::SUB_HAND);
			}
		}

		// if unequip power shard
		if (itemToUnequip->getItemTemplate()->getItemGroup() == ItemGroup::POWER_SHARDS) {
			owner.unsetState(CreatureState::POWERSHARD);
			PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_EMOTION(owner, EmotionType::POWERSHARD_OFF, 0, 0));
		}

		if (itemToUnequip->getItemTemplate()->isStigma()) {
			services::StigmaService::removeStigmaSkills(owner, itemToUnequip->getItemTemplate()->getStigma(), itemToUnequip->getEnchantLevel(),
				true);
		}

		unEquip(itemToUnequip->getEquipmentSlot());

		return itemToUnequip;
	}
}

runtime::Ptr<Item> Equipment::unEquipItem(int32_t itemObjId) {
	return unEquipItem(itemObjId, true);
}

void Equipment::unEquip(int64_t slot) {
	bool updateStats = false;
	std::vector<items::ItemSlot> allSlots = detail::getSlotsFor(slot);
	for (items::ItemSlot itemSlot : allSlots) {
		runtime::Ptr<Item> item = equipment.remove(detail::getSlotIdMask(itemSlot));
		if (!item || !item->isEquipped()) // check isEquipped to avoid duplicate notifyUnequip, since two handed weapons occupy two slots
			continue;
		updateStats = true;
		item->setEquipped(false);
		item->setEquipmentSlot(0);
		owner.getInventory().put(*item);
		setPersistentState(PersistentState::UPDATE_REQUIRED);
		notifyItemUnequip(*item);
	}
	if (updateStats) {
		owner.getLifeStats()->updateCurrentStats();
		owner.getGameStats()->updateStatsAndSpeedVisually();
	}
}

void Equipment::unequip(Item& item) {
	if (item.getItemTemplate()->isTwoHandWeapon()) {
		for (items::ItemSlot slot : detail::getSlotsFor(item.getEquipmentSlot()))
			equipment.remove(detail::getSlotIdMask(slot));
	} else {
		equipment.remove(item.getEquipmentSlot());
	}
	item.setEquipped(false);
}

bool Equipment::checkAvailableEquipSkills(Item& item) {
	std::span<const int32_t> requiredSkills = item.getItemTemplate()->getRequiredSkills();
	if (requiredSkills.empty()) // if no skills required - validate as true
		return true;

	for (int32_t skill : requiredSkills) {
		if (owner.getSkillList()->isSkillPresent(skill))
			return true;
	}

	return false; // FIXME leather skill allows you to wear leather. You don't need cloth skill too!
}

runtime::Ptr<Item> Equipment::getEquippedItemByObjId(int32_t itemObjId) {
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (item->getObjectId() == itemObjId)
				return item;
		}
	}
	return nullptr;
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsByItemId(int32_t value) {
	std::vector<runtime::Ptr<Item>> equippedItemsById;
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (item->getItemTemplate()->getTemplateId() == value)
				equippedItemsById.push_back(item);
		}
	}
	return equippedItemsById;
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItems() {
	SYNCHRONIZED(equipment) {
		// Java: equipment.values().stream().distinct() (Item equals: same object id)
		std::vector<runtime::Ptr<Item>> items;
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (!containsEqual(items, *item))
				items.push_back(item);
		}
		return items;
	}
}

std::unordered_set<int32_t> Equipment::getEquippedItemIds() {
	SYNCHRONIZED(equipment) {
		std::unordered_set<int32_t> ids;
		for (const runtime::Ptr<Item>& item : equipment.values())
			ids.insert(item->getItemId());
		return ids;
	}
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsWithoutStigma() {
	std::vector<runtime::Ptr<Item>> equippedItems;
	std::vector<runtime::Ptr<Item>> twoHanded; // Java HashSet<Item> (Item equals)
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (!detail::isStigma(item->getEquipmentSlot())) {
				if (item->getItemTemplate()->isTwoHandWeapon()) {
					if (containsEqual(twoHanded, *item))
						continue;
					twoHanded.push_back(item);
				}
				equippedItems.push_back(item);
			}
		}
	}
	return equippedItems;
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedForAppearance() {
	std::vector<runtime::Ptr<Item>> equippedItems;
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (detail::isVisible(item->getEquipmentSlot()) && !(item->getItemTemplate()->isTwoHandWeapon() && containsEqual(equippedItems, *item)))
				equippedItems.push_back(item);
		}
	}
	return equippedItems;
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsAllStigma() {
	std::vector<runtime::Ptr<Item>> equippedItems;
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (detail::isStigma(item->getEquipmentSlot()))
				equippedItems.push_back(item);
		}
	}
	return equippedItems;
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsRegularStigma() {
	std::vector<runtime::Ptr<Item>> equippedItems;
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (detail::isRegularStigma(item->getEquipmentSlot()))
				equippedItems.push_back(item);
		}
	}
	return equippedItems;
}

std::vector<runtime::Ptr<Item>> Equipment::getEquippedItemsAdvancedStigma() {
	std::vector<runtime::Ptr<Item>> equippedItems;
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if (detail::isAdvancedStigma(item->getEquipmentSlot()))
				equippedItems.push_back(item);
		}
	}
	return equippedItems;
}

int32_t Equipment::itemSetPartsEquipped(int32_t itemSetTemplateId) {
	int32_t number = 0;
	std::vector<int32_t> counted; // no double counting for accessory and weapons

	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if ((item->getEquipmentSlot() & detail::MAIN_OFF_HAND) != 0 || (item->getEquipmentSlot() & detail::SUB_OFF_HAND) != 0)
				continue;
			const templates::itemset::ItemSetTemplate* setTemplate = item->getItemTemplate()->getItemSet();
			if (setTemplate != nullptr && setTemplate->getId() == itemSetTemplateId && std::ranges::find(counted, item->getItemId()) == counted.end()) {
				counted.push_back(item->getItemId());
				++number;
			}
		}
	}
	return number;
}

void Equipment::onLoadHandler(Item& item) {
	if (!checkAvailableEquipSkills(item)) {
		putItemBackToInventory(item);
		return;
	}
	if (!checkDualWieldRestriction(item, item.getEquipmentSlot())) {
		putItemBackToInventory(item);
		return;
	}
	for (items::ItemSlot slot : detail::getSlotsFor(item.getEquipmentSlot())) { // two slots (main+sub) for two-handed weapons
		if (equipment.putIfAbsent(detail::getSlotIdMask(slot), runtime::Ref<Item>(item))) {
			log.warn("Duplicate equipped item in slot " + std::string(xml::EnumTraits<items::ItemSlot>::names[static_cast<size_t>(slot)]) + " for "
				+ owner.toString());
			putItemBackToInventory(item);
		}
	}
}

void Equipment::putItemBackToInventory(Item& item) {
	item.setEquipped(false);
	item.setEquipmentSlot(0);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	owner.getInventory().put(item);
}

void Equipment::onLoadApplyEquipmentStats() {
	runtime::Ptr<Item> twoHanded;
	SYNCHRONIZED(equipment) {
		for (const runtime::Ptr<Item>& item : equipment.values()) {
			if ((item->getEquipmentSlot() & detail::MAIN_OFF_HAND) == 0 && (item->getEquipmentSlot() & detail::SUB_OFF_HAND) == 0) {
				if (item->getItemTemplate()->isTwoHandWeapon()) {
					if (twoHanded)
						continue;
					twoHanded = item;
				}
				onItemEquipment(*item, owner);
			}
		}
	}
	owner.getLifeStats()->synchronizeWithMaxStats();
}

bool Equipment::isShieldEquipped() {
	runtime::Ptr<Item> subHandItem = equipment.get(detail::SUB_HAND);
	if (!subHandItem)
		return false;
	ItemSubType shieldType = subHandItem->getItemTemplate()->getItemSubType();
	return shieldType == ItemSubType::SHIELD;
}

runtime::Ptr<Item> Equipment::getEquippedShield() {
	runtime::Ptr<Item> subHandItem = equipment.get(detail::SUB_HAND);
	if (!subHandItem)
		return nullptr;
	ItemSubType shieldType = subHandItem->getItemTemplate()->getItemSubType();
	return (shieldType == ItemSubType::SHIELD) ? subHandItem : nullptr;
}

std::optional<templates::item::enums::ItemGroup> Equipment::getMainHandWeaponType() {
	runtime::Ptr<Item> mainHandItem = equipment.get(detail::MAIN_HAND);
	if (!mainHandItem)
		return std::nullopt;

	return mainHandItem->getItemTemplate()->getItemGroup();
}

std::optional<templates::item::enums::ItemGroup> Equipment::getOffHandWeaponType() {
	runtime::Ptr<Item> offHandItem = equipment.get(detail::SUB_HAND);
	runtime::Ptr<Item> mainHandItem = equipment.get(detail::MAIN_HAND);
	if (mainHandItem == offHandItem)
		offHandItem = nullptr;
	if (offHandItem && offHandItem->getItemTemplate()->isWeapon())
		return offHandItem->getItemTemplate()->getItemGroup();

	return std::nullopt;
}

bool Equipment::isPowerShardEquipped() {
	return getMainHandPowerShard() || getOffHandPowerShard();
}

runtime::Ptr<Item> Equipment::getMainHandPowerShard() {
	return equipment.get(detail::POWER_SHARD_RIGHT);
}

runtime::Ptr<Item> Equipment::getOffHandPowerShard() {
	return equipment.get(detail::POWER_SHARD_LEFT);
}

void Equipment::usePowerShard(Item& powerShardItem, int32_t count) {
	decreaseEquippedItemCount(powerShardItem.getObjectId(), count);

	if (powerShardItem.getItemCount() <= 0) { // Search for next same power shards stack
		std::vector<runtime::Ptr<Item>> powerShardStacks =
			owner.getInventory().getItemsByItemId(powerShardItem.getItemTemplate()->getTemplateId());
		if (!powerShardStacks.empty()) {
			equipItem(powerShardStacks[0]->getObjectId(), powerShardItem.getEquipmentSlot());
		} else {
			PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_MSG_WEAPON_BOOST_MODE_BURN_OUT());
			owner.unsetState(CreatureState::POWERSHARD);
		}
	}
}

int64_t Equipment::increaseEquippedItemCount(Item& item, int64_t count) {
	// Only Shards can be increased
	if (item.getItemTemplate()->getItemGroup() != ItemGroup::POWER_SHARDS)
		return count;

	int64_t leftCount = item.increaseItemCount(count);
	services::item::ItemPacketService::updateItemAfterInfoChange(owner, item, services::item::ItemPacketService::ItemUpdateType::STATS_CHANGE);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	return leftCount;
}

void Equipment::decreaseEquippedItemCount(int32_t itemObjId, int32_t count) {
	runtime::Ptr<Item> equippedItem = getEquippedItemByObjId(itemObjId);

	if (equippedItem->getItemCount() >= count)
		equippedItem->decreaseItemCount(count);
	else
		equippedItem->decreaseItemCount(equippedItem->getItemCount());

	if (equippedItem->getItemCount() == 0) {
		dao::InventoryDAO::store(*equippedItem, owner); // must store (delete) before unequip
		unequip(*equippedItem);
		PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_DELETE_ITEM(equippedItem->getObjectId()));
	} else {
		services::item::ItemPacketService::updateItemAfterInfoChange(owner, *equippedItem,
			services::item::ItemPacketService::ItemUpdateType::STATS_CHANGE);
	}
	PacketSendUtility::broadcastPacket(owner,
		network::aion::serverpackets::SM_UPDATE_PLAYER_APPEARANCE(owner.getObjectId(), owner.getEquipment().getEquippedForAppearance()), true);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void Equipment::switchHands() {
	runtime::Ptr<Item> mainHandItem = equipment.get(detail::MAIN_HAND);
	runtime::Ptr<Item> subHandItem = equipment.get(detail::SUB_HAND);
	runtime::Ptr<Item> mainOffHandItem = equipment.get(detail::MAIN_OFF_HAND);
	runtime::Ptr<Item> subOffHandItem = equipment.get(detail::SUB_OFF_HAND);

	std::vector<runtime::Ptr<Item>> equippedWeapon;

	if (mainHandItem)
		equippedWeapon.push_back(mainHandItem);
	if (subHandItem && subHandItem != mainHandItem)
		equippedWeapon.push_back(subHandItem);
	if (mainOffHandItem)
		equippedWeapon.push_back(mainOffHandItem);
	if (subOffHandItem && subOffHandItem != mainOffHandItem)
		equippedWeapon.push_back(subOffHandItem);

	for (const runtime::Ptr<Item>& item : equippedWeapon) {
		unequip(*item);
		PacketSendUtility::sendPacket(owner,
			network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(owner, *item, services::item::ItemPacketService::ItemUpdateType::EQUIP_UNEQUIP));
		if (owner.getGameStats()) {
			if ((item->getEquipmentSlot() & detail::MAIN_HAND) != 0 || (item->getEquipmentSlot() & detail::SUB_HAND) != 0)
				notifyItemUnequip(*item);
		}
	}

	for (const runtime::Ptr<Item>& item : equippedWeapon) {
		int64_t oldSlots = item->getEquipmentSlot();
		if ((oldSlots & detail::RIGHT_HAND) != 0)
			oldSlots ^= detail::RIGHT_HAND;
		if ((oldSlots & detail::LEFT_HAND) != 0)
			oldSlots ^= detail::LEFT_HAND;
		item->setEquipmentSlot(oldSlots);
	}

	for (const runtime::Ptr<Item>& item : equippedWeapon) {
		if (item->getItemTemplate()->isTwoHandWeapon()) {
			for (items::ItemSlot slot : detail::getSlotsFor(item->getEquipmentSlot()))
				equipment.put(detail::getSlotIdMask(slot), runtime::Ref<Item>(item));
		} else {
			equipment.put(item->getEquipmentSlot(), runtime::Ref<Item>(item));
		}
		item->setEquipped(true);
		services::item::ItemPacketService::updateItemAfterEquip(owner, *item);
	}

	if (owner.getGameStats()) {
		for (const runtime::Ptr<Item>& item : equippedWeapon) {
			if ((item->getEquipmentSlot() & detail::MAIN_HAND) != 0 || (item->getEquipmentSlot() & detail::SUB_HAND) != 0)
				notifyItemEquipped(*item);
		}
	}

	owner.getLifeStats()->updateCurrentStats();
	owner.getGameStats()->updateStatsAndSpeedVisually();
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

bool Equipment::isWeaponEquipped(templates::item::enums::ItemSubType subType) {
	runtime::Ptr<Item> weapon = getMainHandWeapon();
	if (weapon && weapon->getItemTemplate()->getItemSubType() == subType)
		return true;
	weapon = getOffHandWeapon();
	if (weapon && weapon->getItemTemplate()->getItemSubType() == subType)
		return true;
	return false;
}

bool Equipment::isDualWeaponEquipped() {
	for (int64_t offhandSlot : {detail::SUB_HAND, detail::MAIN_HAND}) {
		runtime::Ptr<Item> weapon = equipment.get(offhandSlot);
		if (!weapon || !weapon->getItemTemplate()->isOneHandWeapon())
			return false;
	}
	return true;
}

bool Equipment::isSlotEquipped(int64_t slot) {
	return static_cast<bool>(equipment.get(slot));
}

runtime::Ptr<Item> Equipment::getMainHandWeapon() {
	return equipment.get(detail::MAIN_HAND);
}

runtime::Ptr<Item> Equipment::getOffHandWeapon() {
	runtime::Ptr<Item> result = equipment.get(detail::SUB_HAND);
	if (getMainHandWeapon() == result)
		return nullptr;
	return result;
}

void Equipment::setPersistentState(PersistentState persistentStateValue) {
	persistentState.set(persistentStateValue);
}

bool Equipment::soulBindItem(Player& player, Item& item, int64_t slot) {
	if (!player.getInventory().getItemByObjId(item.getObjectId()))
		return false;
	if (player.isDead()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::DEAD)));
		return false;
	} else if (player.isInPlayerMode(actions::PlayerMode::RIDE)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::RIDING)));
		return false;
	} else if (player.isInState(CreatureState::CHAIR)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::SITTING)));
		return false;
	} else if (player.isInState(CreatureState::RESTING)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::RESTING)));
		return false;
	} else if (player.isInState(CreatureState::GLIDING)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::GLIDING)));
		return false;
	} else if (player.isInState(CreatureState::FLYING)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::FREE_FLYING)));
		return false;
	} else if (player.isInState(CreatureState::WEAPON_EQUIPPED)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_INVALID_STANCE(getL10n(ActionState::COMBAT)));
		return false;
	}

	runtime::Ref<Equipment_RequestResponseHandler> responseHandler = Equipment_RequestResponseHandler::create(player, *this, item, slot);

	bool requested = player.getResponseRequester().putRequest(network::aion::serverpackets::SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND,
		runtime::Ptr<RequestResponseHandler>(responseHandler));
	if (requested) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_QUESTION_WINDOW(
												 network::aion::serverpackets::SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, 0, 0, item.getL10n()));
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_CLOSE_OTHER_MSG_BOX_AND_RETRY());
	}
	return false;
}

bool Equipment::verifyRankLimits(Item& item) {
	int32_t rank = rankId(owner.getAbyssRank()->getRank());
	if (!item.getItemTemplate()->getUseLimits()->verifyRank(rank))
		return false;
	if (item.getFusionedItemTemplate() != nullptr)
		return item.getFusionedItemTemplate()->getUseLimits()->verifyRank(rank);
	return true;
}

void Equipment::checkRankLimitItems() {
	for (const runtime::Ptr<Item>& item : getEquippedItems()) {
		if (!verifyRankLimits(*item)) {
			unEquipItem(item->getObjectId(), false);
			PacketSendUtility::sendPacket(owner, SM_SYSTEM_MESSAGE::STR_MSG_UNEQUIP_RANKITEM(item->getL10n()));
			// TODO: Check retail what happens with full inv and the task msgs.
		}
	}
}

} // namespace aion::gameserver::model::gameobjects::player
