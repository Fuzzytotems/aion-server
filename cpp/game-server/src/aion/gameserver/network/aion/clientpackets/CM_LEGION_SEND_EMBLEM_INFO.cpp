#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_SEND_EMBLEM_INFO.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_LEGION_SEND_EMBLEM_INFO::CM_LEGION_SEND_EMBLEM_INFO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_SEND_EMBLEM_INFO.java:25-27
void CM_LEGION_SEND_EMBLEM_INFO::readImpl() {
	legionId = readD();
}

// Java CM_LEGION_SEND_EMBLEM_INFO.java:30-38
void CM_LEGION_SEND_EMBLEM_INFO::runImpl() {
	if (!getConnection()->getActivePlayer())
		return;
	const runtime::Ptr<model::team::legion::Legion> legion = services::LegionService::getInstance().getLegion(legionId);
	if (legion)
		sendPacket(serverpackets::SM_LEGION_SEND_EMBLEM(legionId, *legion->getLegionEmblem(), 0, legion->getName())); // send only info without following EMBLEM_DATA packets
}

AION_CLIENT_PACKET(CM_LEGION_SEND_EMBLEM_INFO);

} // namespace aion::gameserver::network::aion::clientpackets
