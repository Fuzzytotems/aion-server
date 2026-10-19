#include "aion/gameserver/network/aion/clientpackets/CM_GODSTONE_SOCKET.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/utils/PositionUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GODSTONE_SOCKET::CM_GODSTONE_SOCKET(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_GODSTONE_SOCKET.java:27-31
void CM_GODSTONE_SOCKET::readImpl() {
	npcObjectId = readD();
	weaponId = readD();
	stoneId = readD();
}

// Java CM_GODSTONE_SOCKET.java:34-39
void CM_GODSTONE_SOCKET::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	runtime::Ptr<model::gameobjects::VisibleObject> npc = player->getTarget();
	if (runtime::Ptr<model::gameobjects::Npc> targetNpc = runtime::as<model::gameobjects::Npc>(npc);
		targetNpc && npc->getObjectId() == npcObjectId && utils::PositionUtil::isInTalkRange(*player, *targetNpc))
		services::item::ItemSocketService::socketGodstone(*player, player->getEquipment().getEquippedItemByObjId(weaponId), stoneId);
}

// no AION_CLIENT_PACKET marker: the class is registered under no opcode (see the header)

} // namespace aion::gameserver::network::aion::clientpackets
