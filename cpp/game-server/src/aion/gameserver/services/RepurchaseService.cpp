#include "aion/gameserver/services/RepurchaseService.h"

#include <algorithm>
#include <string>
#include <utility>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/trade/RepurchaseList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

RepurchaseService::RepurchaseService() = default;

void RepurchaseService::addRepurchaseItems(model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	runtime::Ref<runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>> itemSet =
		runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>::create(AION_LOCK_CLASS(RepurchaseService::repurchaseItems#value));
	for (const runtime::Ptr<model::gameobjects::Item>& item : items)
		itemSet->add(runtime::Ref<model::gameobjects::Item>(*item));
	repurchaseItems.put(player.getObjectId(), std::move(itemSet));
}

void RepurchaseService::removeRepurchaseItems(model::gameobjects::player::Player& player) {
	repurchaseItems.remove(player.getObjectId());
}

std::unordered_set<runtime::Ptr<model::gameobjects::Item>> RepurchaseService::getRepurchaseItems(int32_t playerObjectId) {
	std::unordered_set<runtime::Ptr<model::gameobjects::Item>> result; // Java: getOrDefault(playerObjectId, Collections.emptySet())
	if (runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>> items = repurchaseItems.get(playerObjectId)) {
		for (const runtime::Ptr<model::gameobjects::Item>& item : items->snapshot())
			result.insert(item);
	}
	return result;
}

bool RepurchaseService::canRepurchase(model::gameobjects::player::Player& player, int32_t itemObjectId) {
	return std::ranges::any_of(getRepurchaseItems(player.getObjectId()),
		[itemObjectId](const runtime::Ptr<model::gameobjects::Item>& item) { return item->getObjectId() == itemObjectId; });
}

// Java RepurchaseService.java:47-69
void RepurchaseService::repurchaseFromShop(model::gameobjects::player::Player& player, model::trade::RepurchaseList& repurchaseList) {
	if (!restrictions::PlayerRestrictions::canTrade(runtime::Ptr<model::gameobjects::player::Player>(player))) {
		return;
	}
	// a player without sold items has no entry: Java's null set throws its NullPointerException at the first lookup below (Ptr's operator->)
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>> items = repurchaseItems.get(player.getObjectId());
	for (int32_t itemObjectId : repurchaseList.getRepurchaseItems()) {
		if (player.getInventory().isFull()) {
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR());
			break;
		}

		runtime::Ptr<model::gameobjects::Item> repurchaseItem;
		for (const runtime::Ptr<model::gameobjects::Item>& item : items->snapshot()) {
			if (item->getObjectId() == itemObjectId) {
				repurchaseItem = item; // Java: findAny() (object ids are unique, so any is the one)
				break;
			}
		}
		if (repurchaseItem) {
			if (player.getInventory().tryDecreaseKinah(repurchaseItem->getRepurchasePrice())) {
				item::ItemService::addItem(player, *repurchaseItem);
				items->remove(repurchaseItem);
			} else {
				utils::audit::AuditLogger::log(player, "tried to repurchase item " + std::to_string(repurchaseItem->getItemId()) + ", count: " +
					std::to_string(repurchaseItem->getItemCount()) + " without kinah");
			}
		}
	}
}

RepurchaseService& RepurchaseService::getInstance() {
	static RepurchaseService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
