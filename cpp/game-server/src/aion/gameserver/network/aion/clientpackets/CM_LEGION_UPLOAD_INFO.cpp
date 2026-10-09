#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_UPLOAD_INFO.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/LegionEmblemType.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {


CM_LEGION_UPLOAD_INFO::CM_LEGION_UPLOAD_INFO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_UPLOAD_INFO.java:30-36
void CM_LEGION_UPLOAD_INFO::readImpl() {
	totalSize = readD();
	alpha = readUC();
	red = readUC();
	green = readUC();
	blue = readUC();
}

// Java CM_LEGION_UPLOAD_INFO.java:39-42
void CM_LEGION_UPLOAD_INFO::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::LegionService::getInstance().uploadEmblemInfo(*activePlayer, totalSize, alpha, red, green, blue,
		model::team::legion::LegionEmblemType::CUSTOM);
}

AION_CLIENT_PACKET(CM_LEGION_UPLOAD_INFO);

} // namespace aion::gameserver::network::aion::clientpackets
