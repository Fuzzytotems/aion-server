#include "aion/gameserver/network/aion/clientpackets/CM_MARK_FRIENDLIST.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MARK_FRIENDLIST.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_MARK_FRIENDLIST::CM_MARK_FRIENDLIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MARK_FRIENDLIST.java:22-25
void CM_MARK_FRIENDLIST::readImpl() {
	// nothing to read
}

// Java CM_MARK_FRIENDLIST.java:27-30
void CM_MARK_FRIENDLIST::runImpl() {
	sendPacket(serverpackets::SM_MARK_FRIENDLIST());
}

AION_CLIENT_PACKET(CM_MARK_FRIENDLIST);

} // namespace aion::gameserver::network::aion::clientpackets
