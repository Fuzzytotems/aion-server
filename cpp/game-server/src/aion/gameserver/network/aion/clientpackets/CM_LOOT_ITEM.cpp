#include "aion/gameserver/network/aion/clientpackets/CM_LOOT_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/drop/DropService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_LOOT_ITEM::CM_LOOT_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LOOT_ITEM.java:22-26
void CM_LOOT_ITEM::readImpl() {
	targetObjectId = readD();
	index = readUC();
}

// Java CM_LOOT_ITEM.java:28-34
void CM_LOOT_ITEM::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	services::drop::DropService::getInstance().requestDropItem(*player, targetObjectId, index);
}

AION_CLIENT_PACKET(CM_LOOT_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
