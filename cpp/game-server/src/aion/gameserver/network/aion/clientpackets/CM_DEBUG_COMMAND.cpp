#include "aion/gameserver/network/aion/clientpackets/CM_DEBUG_COMMAND.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_DEBUG_COMMAND::CM_DEBUG_COMMAND(int32_t opcode, const StateSet& validStates) : AbstractGmCommandPacket(opcode, validStates) {
}

// Java CM_DEBUG_COMMAND.java:23-26: Java's string concatenation of the player is Player.toString (null when no player is active)
void CM_DEBUG_COMMAND::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG")
		.info((player ? player->toString() : std::string("null")) + " sent debug command ////" + command);
}

AION_CLIENT_PACKET(CM_DEBUG_COMMAND);

} // namespace aion::gameserver::network::aion::clientpackets
