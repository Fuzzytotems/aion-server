#include "aion/gameserver/services/item/ItemSocketService.h"

#include <string>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::item {

namespace {

using model::TaskId;
using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::templates::item::ItemTemplate;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

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
	AION_UNPORTED();
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, int32_t slotId, bool useFusionSlots) {
	AION_UNPORTED();
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::insertManaStoneIntoNextPossibleSlot(model::gameobjects::Item& item, int32_t manaStoneItemId, int32_t maxSlots, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, model::templates::item::enums::ItemGroup manastoneCategory, int32_t specialSlotCount) {
	AION_UNPORTED();
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::insertManastoneIntoSlot(model::gameobjects::Item& item, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, int32_t manastoneId, int32_t slotId) {
	AION_UNPORTED();
}

void ItemSocketService::copyFusionStones(model::gameobjects::Item& source, model::gameobjects::Item& target) {
	AION_UNPORTED();
}

void ItemSocketService::removeManastone(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t slotNum, bool isFusionSocket) {
	AION_UNPORTED();
}

void ItemSocketService::removeAllManastone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> item) {
	AION_UNPORTED();
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
