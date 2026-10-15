#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_UPLOAD_EMBLEM.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_LEGION_UPLOAD_EMBLEM::CM_LEGION_UPLOAD_EMBLEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_UPLOAD_EMBLEM.java:32-36
void CM_LEGION_UPLOAD_EMBLEM::readImpl() {
	size = readD();
	data = readB(size); // Java: data = new byte[size]; data = readB(size)
}

// Java CM_LEGION_UPLOAD_EMBLEM.java:39-46
void CM_LEGION_UPLOAD_EMBLEM::runImpl() {
	if (!getConnection()->getActivePlayer())
		return;
	if (!data.empty()) {
		services::LegionService::getInstance().uploadEmblemData(*getConnection()->getActivePlayer(), size, data);
	}
}

AION_CLIENT_PACKET(CM_LEGION_UPLOAD_EMBLEM);

} // namespace aion::gameserver::network::aion::clientpackets
