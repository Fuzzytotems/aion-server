#include "aion/gameserver/network/aion/clientpackets/CM_MACRO_CREATE.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_RESULT.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/player/PlayerService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_MACRO_CREATE::CM_MACRO_CREATE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MACRO_CREATE.java:40-44
void CM_MACRO_CREATE::readImpl() {
	macroPosition = readUC();
	macroXML = readS();
}

// Java CM_MACRO_CREATE.java:46-51
void CM_MACRO_CREATE::runImpl() {
	services::player::PlayerService::addMacro(*getConnection()->getActivePlayer(), macroPosition, macroXML);
	sendPacket(serverpackets::SM_MACRO_RESULT::SM_MACRO_CREATED);
}

AION_CLIENT_PACKET(CM_MACRO_CREATE);

} // namespace aion::gameserver::network::aion::clientpackets
