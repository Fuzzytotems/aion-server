#include "aion/gameserver/network/aion/clientpackets/CM_GROUP_LOOT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/drop/DropDistributionService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GROUP_LOOT::CM_GROUP_LOOT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_GROUP_LOOT.java:44-56
void CM_GROUP_LOOT::readImpl() {
	groupId = readD();
	index = readD();
	unk1 = readD();
	itemId = readD();
	unk2 = readUC();
	unk3 = readUC(); // 3.0
	unk4 = readUC(); // 3.5
	npcObjId = readD();
	distributionMode = readUC(); // 2: Roll 3: Bid
	roll = readD();              // 0: Never Rolled 1: Rolled
	bid = readQ();               // 0: No Bid else bid amount
}

// Java CM_GROUP_LOOT.java:59-64
void CM_GROUP_LOOT::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	services::drop::DropDistributionService::getInstance().handleRollOrBid(player, distributionMode, roll, bid, itemId, npcObjId, index);
}

AION_CLIENT_PACKET(CM_GROUP_LOOT);

} // namespace aion::gameserver::network::aion::clientpackets
