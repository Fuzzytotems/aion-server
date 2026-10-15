#include "aion/gameserver/network/aion/clientpackets/CM_SPLIT_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemSplitService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_SPLIT_ITEM::CM_SPLIT_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SPLIT_ITEM.java:26-34
void CM_SPLIT_ITEM::readImpl() {
	sourceItemObjId = readD();
	itemAmount = readQ();
	sourceStorageType = readC();
	destinationItemObjId = readD();
	destinationStorageType = readC();
	slotNum = readH();
}

// Java CM_SPLIT_ITEM.java:36-40
void CM_SPLIT_ITEM::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	services::item::ItemSplitService::splitItem(*player, sourceItemObjId, destinationItemObjId, itemAmount, slotNum, sourceStorageType,
		destinationStorageType);
}

AION_CLIENT_PACKET(CM_SPLIT_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
