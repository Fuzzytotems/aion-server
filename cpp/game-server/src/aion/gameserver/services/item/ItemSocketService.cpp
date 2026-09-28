#include "aion/gameserver/services/item/ItemSocketService.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::item {

namespace {

using model::TaskId;
using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::items::ManaStone;
using model::templates::item::ItemTemplate;
using model::templates::item::enums::ItemGroup;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;
using PersistentState = model::gameobjects::Persistable_PersistentState;
/** Java Set<ManaStone>: the item's live stone set (a TreeSet ordered by slot, Item.java) */
using StoneSet = runtime::RcTreeSet<Ref<ManaStone>>;

/** Java int subtraction (wraps) */
int32_t javaSub(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java DataManager.ITEM_DATA.getItemTemplate(itemId) dereferenced right away (NullPointerException for an unknown item) */
const ItemTemplate& itemTemplateOf(int32_t itemId) {
	const ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("ITEM_DATA.getItemTemplate(" + std::to_string(itemId) + ")");
	return *itemTemplate;
}

/** Java item.getFusionedItemTemplate() dereferenced right away (NullPointerException for an item without a fused weapon) */
const ItemTemplate& fusionedTemplateOf(const Item& item) {
	const ItemTemplate* fusioned = item.getFusionedItemTemplate();
	if (fusioned == nullptr)
		throw runtime::NullPointerException("item.getFusionedItemTemplate()");
	return *fusioned;
}

} // namespace

/**
 * Java: the anonymous ItemUseObserver of socketGodstone (ItemSocketService.java:177-187, fieldmap key ItemSocketService$1), stored in the
 * player's ObserveController until the task or abort() removes it (cycles.toml "ItemSocketService$1#player" / "#weapon": cpp-breaker, the
 * logout breaker's ObserveController::clearWithoutNotify drops it). The item template is immortal static data (fieldmap: captured local
 * template reference).
 */
struct ItemSocketService_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player (line 180)
	const Ref<Item> weapon; // captured param Item weapon (line 182)
	const ItemTemplate* itemTemplate; // captured local ItemTemplate itemTemplate (line 184)
	const int32_t stoneId; // captured param int stoneId (line 184)

	static Ref<ItemSocketService_ItemUseObserver> create(Player& player, Item& weapon, const ItemTemplate* itemTemplate, int32_t stoneId) {
		return runtime::makeRef<ItemSocketService_ItemUseObserver>(player, weapon, itemTemplate, stoneId);
	}

	void abort() override {
		player->getObserveController()->removeObserver(*this);
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_GIVE_PROC_CANCEL(weapon->getL10n()));
		PacketSendUtility::broadcastPacketAndReceive(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), stoneId, itemTemplate->getTemplateId(), 0, 3, 0));
	}

protected:
	ItemSocketService_ItemUseObserver(Player& playerValue, Item& weaponValue, const ItemTemplate* itemTemplateValue, int32_t stoneIdValue)
		: player(Ref<Player>(playerValue)), weapon(Ref<Item>(weaponValue)), itemTemplate(itemTemplateValue), stoneId(stoneIdValue) {}
	~ItemSocketService_ItemUseObserver() override = default;
};

runtime::Ptr<model::items::ManaStone> ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, bool useFusionSlots) {
	if (!item)
		return nullptr;
	int32_t maxSlots = item->getSockets(useFusionSlots);
	Ptr<StoneSet> manaStones = useFusionSlots ? item->getFusionStones() : item->getItemStones();
	if (manaStones->size() > maxSlots)
		return nullptr;
	ItemGroup manaStoneCategory = itemTemplateOf(manaStoneItemId).getItemGroup();
	int32_t specialSlotCount = useFusionSlots ? fusionedTemplateOf(*item).getSpecialSlots() : item->getItemTemplate()->getSpecialSlots();
	return insertManaStoneIntoNextPossibleSlot(*item, manaStoneItemId, maxSlots, *manaStones, manaStoneCategory, specialSlotCount);
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, int32_t slotId, bool useFusionSlots) {
	if (!item)
		return nullptr;
	Ptr<StoneSet> manaStones = useFusionSlots ? item->getFusionStones() : item->getItemStones();
	if (manaStones->size() >= Item::MAX_BASIC_STONES)
		return nullptr;
	return insertManastoneIntoSlot(*item, *manaStones, manaStoneItemId, slotId);
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::insertManaStoneIntoNextPossibleSlot(model::gameobjects::Item& item, int32_t manaStoneItemId, int32_t maxSlots, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, model::templates::item::enums::ItemGroup manastoneCategory, int32_t specialSlotCount) {
	if (manastoneCategory == ItemGroup::SPECIAL_MANASTONE && specialSlotCount == 0)
		return nullptr;

	int32_t specialSlotsOccupied = 0;
	int32_t normalSlotsOccupied = 0;
	std::unordered_set<int32_t> allSlots;
	for (const Ptr<ManaStone>& ms : manaStones) {
		ItemGroup category = itemTemplateOf(ms->getItemId()).getItemGroup();
		if (category == ItemGroup::SPECIAL_MANASTONE)
			specialSlotsOccupied++;
		else
			normalSlotsOccupied++;
		allSlots.insert(ms->getSlot());
	}

	if ((manastoneCategory == ItemGroup::SPECIAL_MANASTONE && specialSlotsOccupied >= specialSlotCount)
		|| (manastoneCategory == ItemGroup::MANASTONE && normalSlotsOccupied >= javaSub(maxSlots, specialSlotCount)))
		return nullptr;

	int32_t start = manastoneCategory == ItemGroup::SPECIAL_MANASTONE ? 0 : specialSlotCount;
	int32_t end = manastoneCategory == ItemGroup::SPECIAL_MANASTONE ? specialSlotCount : maxSlots;
	int32_t nextSlot = start;
	bool slotFound = false;
	for (; nextSlot < end; nextSlot++) {
		if (!allSlots.contains(nextSlot)) {
			slotFound = true;
			break;
		}
	}
	if (!slotFound)
		return nullptr;
	return insertManastoneIntoSlot(item, manaStones, manaStoneItemId, nextSlot);
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::insertManastoneIntoSlot(model::gameobjects::Item& item, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, int32_t manastoneId, int32_t slotId) {
	item.removeRemainingTuningCountIfPossible();
	Ref<ManaStone> stone = ManaStone::create(item.getObjectId(), manastoneId, slotId, PersistentState::NEW);
	// the borrow is taken while `stone` still holds the object: a stone the set refuses (a stone of that slot is there) is returned like Java's
	Ptr<ManaStone> added(stone);
	manaStones.add(std::move(stone));
	return added;
}

void ItemSocketService::copyFusionStones(model::gameobjects::Item& source, model::gameobjects::Item& target) {
	if (source.hasManaStones()) {
		for (const Ptr<ManaStone>& manaStone : *source.getItemStones())
			target.getFusionStones()->add(ManaStone::create(target.getObjectId(), manaStone->getItemId(), manaStone->getSlot(), PersistentState::NEW));
		target.removeRemainingTuningCountIfPossible();
	}
}

void ItemSocketService::removeManastone(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t slotNum, bool isFusionSocket) {
	model::items::storage::Storage& inventory = player.getInventory();
	Ptr<Item> item = inventory.getItemByObjId(itemObjId);
	if (!item) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_TARGET_ITEM());
		return;
	}

	bool hasManaStones = isFusionSocket ? item->hasFusionStones() : item->hasManaStones();
	if (!hasManaStones) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NO_OPTION_TO_REMOVE(item->getL10n()));
		return;
	}

	Ptr<StoneSet> itemStones = isFusionSocket ? item->getFusionStones() : item->getItemStones();
	// Java: itemStones.stream().filter(ms -> ms.getSlot() == slotNum).findFirst().orElse(null)
	Ptr<ManaStone> manaStoneToRemove;
	for (const Ptr<ManaStone>& ms : *itemStones) {
		if (ms->getSlot() == slotNum) {
			manaStoneToRemove = ms;
			break;
		}
	}
	if (!manaStoneToRemove) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_INVALID_OPTION_SLOT_NUMBER(item->getL10n()));
		return;
	}

	int64_t price = trade::PricesService::getPriceForService(650, player.getRace());
	if (player.getInventory().tryDecreaseKinah(price)) {
		manaStoneToRemove->setPersistentState(PersistentState::DELETED);
		if (isFusionSocket) {
			dao::ItemStoneListDAO::storeFusionStone({manaStoneToRemove});
		} else {
			dao::ItemStoneListDAO::storeManaStones({manaStoneToRemove});
		}
		itemStones->remove(manaStoneToRemove);

		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_SUCCEED(item->getL10n()));
		ItemPacketService::updateItemAfterInfoChange(player, *item);
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REMOVE_ITEM_OPTION_NOT_ENOUGH_GOLD(item->getL10n()));
	}
}

void ItemSocketService::removeAllManastone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> item) {
	if (!item || !item->hasManaStones())
		return;

	Ptr<StoneSet> itemStones = item->getItemStones();
	for (const Ptr<ManaStone>& ms : *itemStones)
		ms->setPersistentState(PersistentState::DELETED);
	// Java hands the live set to the DAO; the DAO takes a set of borrows of the same stones
	std::vector<Ptr<ManaStone>> stones = itemStones->snapshot();
	dao::ItemStoneListDAO::storeManaStones(std::unordered_set<Ptr<ManaStone>>(stones.begin(), stones.end()));
	itemStones->clear();

	ItemPacketService::updateItemAfterInfoChange(player, *item);
}

void ItemSocketService::socketGodstone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> weapon, int32_t stoneId) {
	if (!weapon) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_TARGET_ITEM());
		return;
	}

	if (!weapon->canSocketGodstone()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NOT_PROC_GIVABLE_ITEM(weapon->getL10n()));
		utils::audit::AuditLogger::log(player, "tried to insert godstone in not compatible item " + std::to_string(weapon->getItemId()));
		return;
	}

	Ptr<Item> godstone = player.getInventory().getItemByObjId(stoneId);
	if (!godstone) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_PROC_GIVE_ITEM());
		return;
	}

	const ItemTemplate* itemTemplate = godstone->getItemTemplate();
	if (itemTemplate->getGodstoneInfo() == nullptr) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_PROC_GIVE_ITEM());
		return;
	}

	Ref<ItemSocketService_ItemUseObserver> observer = ItemSocketService_ItemUseObserver::create(player, *weapon, itemTemplate, stoneId);

	player.getObserveController()->attach(*observer);

	PacketSendUtility::broadcastPacketAndReceive(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), stoneId, itemTemplate->getTemplateId(), 2000, 0, 0));

	// Java lambda ItemSocketService.java:193-206 (fieldmap ItemSocketService@L193:92: pins observer, player and weapon). Java captures the
	// template; the task reads only its id, captured as a value (lint L5 takes no pointer captures)
	ItemSocketService_ItemUseObserver& itemUseObserver = *observer;
	Item& weaponItem = *weapon;
	const int32_t templateId = itemTemplate->getTemplateId();
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({&player, &itemUseObserver, &weaponItem},
			[&player, &itemUseObserver, &weaponItem, stoneId, templateId] {
				player.getObserveController()->removeObserver(itemUseObserver);

				PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), stoneId, templateId, 0, 1, 0));

				if (!player.getInventory().decreaseByObjectId(stoneId, 1))
					return;

				weaponItem.addGodStone(templateId);
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM(weaponItem.getL10n()));

				ItemPacketService::updateItemAfterInfoChange(player, weaponItem);
			},
			2000));
}

} // namespace aion::gameserver::services::item
