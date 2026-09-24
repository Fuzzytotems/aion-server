#include "aion/gameserver/network/aion/clientpackets/CM_START_LOOT.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/drop/DropService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_START_LOOT");

using model::gameobjects::player::Player;
using services::drop::DropService;

CM_START_LOOT::CM_START_LOOT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_START_LOOT.java:34-38
void CM_START_LOOT::readImpl() {
	targetObjectId = readD(); // empty
	action = readC();
}

// Java CM_START_LOOT.java:40-54
void CM_START_LOOT::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	switch (action) {
		case 0: // open
			DropService::getInstance().requestDropList(player, targetObjectId);
			break;
		case 1: // close
			DropService::getInstance().closeDropList(*player, targetObjectId);
			break;
		default:
			// Java string concatenation: String.valueOf(player), "null" for null; the byte prints as a signed number
			log.warn((player ? player->toString() : std::string("null")) + " sent unknown loot action type " + std::to_string(action));
	}
}

AION_CLIENT_PACKET(CM_START_LOOT);

} // namespace aion::gameserver::network::aion::clientpackets
