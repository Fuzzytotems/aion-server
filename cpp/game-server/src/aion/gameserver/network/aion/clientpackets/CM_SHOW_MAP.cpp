#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_MAP.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_SHOW_MAP");

CM_SHOW_MAP::CM_SHOW_MAP(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SHOW_MAP.java:26-28
void CM_SHOW_MAP::readImpl() {
	action = readC();
}

// Java CM_SHOW_MAP.java:31-43
void CM_SHOW_MAP::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	switch (action) {
		case 0:
			services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().intruderScan(*player);
			break;
		case 1:
			// TODO unk
			break;
		default:
			log.warn(player->toString() + " sent unknown show map action type: " + std::to_string(action));
	}
}

AION_CLIENT_PACKET(CM_SHOW_MAP);

} // namespace aion::gameserver::network::aion::clientpackets
