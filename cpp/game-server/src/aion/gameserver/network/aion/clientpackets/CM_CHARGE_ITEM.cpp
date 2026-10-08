#include "aion/gameserver/network/aion/clientpackets/CM_CHARGE_ITEM.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemChargeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHARGE_ITEM::CM_CHARGE_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHARGE_ITEM.java:27-35
void CM_CHARGE_ITEM::readImpl() {
	targetNpcObjectId = readD();
	chargeLevel = readUC();
	const int32_t itemsSize = readUH();
	itemObjectIds.clear();
	for (int32_t i = 0; i < itemsSize; i++)
		itemObjectIds.push_back(readD());
}

// Java CM_CHARGE_ITEM.java:37-50
void CM_CHARGE_ITEM::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player->isTargeting(targetNpcObjectId)) {
		return; // TODO audit?
	}
	std::vector<runtime::Ptr<model::gameobjects::Item>> itemsToCharge;
	for (int32_t itemObjId : itemObjectIds) {
		runtime::Ptr<model::gameobjects::Item> item = player->getInventory().getItemByObjId(itemObjId);
		if (item != nullptr)
			itemsToCharge.push_back(item);
	}
	services::item::ItemChargeService::chargeItems(*player, itemsToCharge, chargeLevel, false, true);
}

AION_CLIENT_PACKET(CM_CHARGE_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
