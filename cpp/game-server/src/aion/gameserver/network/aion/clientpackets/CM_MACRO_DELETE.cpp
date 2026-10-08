#include "aion/gameserver/network/aion/clientpackets/CM_MACRO_DELETE.h"

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

CM_MACRO_DELETE::CM_MACRO_DELETE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MACRO_DELETE.java:37-40
void CM_MACRO_DELETE::readImpl() {
	macroPosition = readUC();
}

// Java CM_MACRO_DELETE.java:42-47
void CM_MACRO_DELETE::runImpl() {
	services::player::PlayerService::removeMacro(*getConnection()->getActivePlayer(), macroPosition);
	sendPacket(serverpackets::SM_MACRO_RESULT::SM_MACRO_DELETED);
}

AION_CLIENT_PACKET(CM_MACRO_DELETE);

} // namespace aion::gameserver::network::aion::clientpackets
