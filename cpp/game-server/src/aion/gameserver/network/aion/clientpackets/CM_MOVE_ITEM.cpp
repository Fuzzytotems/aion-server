#include "aion/gameserver/network/aion/clientpackets/CM_MOVE_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemMoveService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_MOVE_ITEM::CM_MOVE_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MOVE_ITEM.java:24-30
void CM_MOVE_ITEM::readImpl() {
	itemObjId = readD();
	source = readC();      // FROM (0 - player inventory, 1 - regular warehouse, 2 - account warehouse, 3 - legion warehouse)
	destination = readC(); // TO
	slot = readH();
}

// Java CM_MOVE_ITEM.java:32-36
void CM_MOVE_ITEM::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	services::item::ItemMoveService::moveItem(*player, itemObjId, source, destination, slot);
}

AION_CLIENT_PACKET(CM_MOVE_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
