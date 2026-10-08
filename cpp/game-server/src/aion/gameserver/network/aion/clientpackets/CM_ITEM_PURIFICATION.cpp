#include "aion/gameserver/network/aion/clientpackets/CM_ITEM_PURIFICATION.h"

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
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemPurificationService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_ITEM_PURIFICATION::CM_ITEM_PURIFICATION(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_ITEM_PURIFICATION.java:29-39
void CM_ITEM_PURIFICATION::readImpl() {
	playerObjectId = readD();
	upgradedItemObjectId = readD();
	resultItemId = readD();
	requireItemObjectId1 = readD();
	requireItemObjectId2 = readD();
	requireItemObjectId3 = readD();
	requireItemObjectId4 = readD();
	requireItemObjectId5 = readD();
}

// Java CM_ITEM_PURIFICATION.java:41-56
void CM_ITEM_PURIFICATION::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player == nullptr)
		return;
	runtime::Ptr<model::gameobjects::Item> baseItem = player->getInventory().getItemByObjId(upgradedItemObjectId);
	if (baseItem == nullptr) // Java passes null on: isPurificationAllowed's first use of baseItem throws
		throw runtime::NullPointerException("Inventory.getItemByObjId(" + std::to_string(upgradedItemObjectId) + ")");
	if (!services::item::ItemPurificationService::isPurificationAllowed(*player, *baseItem, resultItemId))
		return;
	if (!services::item::ItemPurificationService::decreaseMaterials(*player, *baseItem, resultItemId))
		return;
	services::item::ItemPurificationService::upgradeItem(*player, *baseItem, resultItemId);
}

AION_CLIENT_PACKET(CM_ITEM_PURIFICATION);

} // namespace aion::gameserver::network::aion::clientpackets
