#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_MODIFY_EMBLEM.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblemTypeInfo.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using model::team::legion::LegionEmblemType;


CM_LEGION_MODIFY_EMBLEM::CM_LEGION_MODIFY_EMBLEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_MODIFY_EMBLEM.java:32-41
void CM_LEGION_MODIFY_EMBLEM::readImpl() {
	legionId = readD();
	emblemId = readUC();
	// Java: readUC() == LegionEmblemType.DEFAULT.getValue() - an int against a byte widened to int
	emblemType = (readUC() == getValue(LegionEmblemType::DEFAULT)) ? LegionEmblemType::DEFAULT : LegionEmblemType::CUSTOM;
	alpha = readUC();
	red = readUC();
	green = readUC();
	blue = readUC();
}

// Java CM_LEGION_MODIFY_EMBLEM.java:44-48
void CM_LEGION_MODIFY_EMBLEM::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	if (activePlayer->isLegionMember() && activePlayer->getLegion()->getLegionId() == legionId)
		services::LegionService::getInstance().storeLegionEmblem(*activePlayer, emblemId, alpha, red, green, blue, emblemType);
}

AION_CLIENT_PACKET(CM_LEGION_MODIFY_EMBLEM);

} // namespace aion::gameserver::network::aion::clientpackets
