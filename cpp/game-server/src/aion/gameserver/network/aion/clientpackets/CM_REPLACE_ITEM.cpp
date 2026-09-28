#include "aion/gameserver/network/aion/clientpackets/CM_REPLACE_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemMoveService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_REPLACE_ITEM::CM_REPLACE_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_REPLACE_ITEM.java:24-30
void CM_REPLACE_ITEM::readImpl() {
	sourceStorageType = readC();
	sourceItemObjId = readD();
	replaceStorageType = readC();
	replaceItemObjId = readD();
}

// Java CM_REPLACE_ITEM.java:32-36
void CM_REPLACE_ITEM::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	services::item::ItemMoveService::switchItemsInStorages(*player, sourceStorageType, sourceItemObjId, replaceStorageType, replaceItemObjId);
}

AION_CLIENT_PACKET(CM_REPLACE_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
