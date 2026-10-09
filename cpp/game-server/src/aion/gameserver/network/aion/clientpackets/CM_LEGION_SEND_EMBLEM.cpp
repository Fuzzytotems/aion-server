#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_SEND_EMBLEM.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_LEGION_SEND_EMBLEM::CM_LEGION_SEND_EMBLEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_SEND_EMBLEM.java:25-27
void CM_LEGION_SEND_EMBLEM::readImpl() {
	legionId = readD();
}

// Java CM_LEGION_SEND_EMBLEM.java:30-34
void CM_LEGION_SEND_EMBLEM::runImpl() {
	const runtime::Ptr<model::team::legion::Legion> legion = services::LegionService::getInstance().getLegion(legionId);
	if (legion)
		services::LegionService::getInstance().sendEmblemData(*getConnection()->getActivePlayer(), *legion->getLegionEmblem(), legionId, legion->getName());
}

AION_CLIENT_PACKET(CM_LEGION_SEND_EMBLEM);

} // namespace aion::gameserver::network::aion::clientpackets
