#include "aion/gameserver/network/aion/clientpackets/CM_GF_WEBSHOP_TOKEN_REQUEST.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GF_WEBSHOP_TOKEN_RESPONSE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GF_WEBSHOP_TOKEN_REQUEST::CM_GF_WEBSHOP_TOKEN_REQUEST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_GF_WEBSHOP_TOKEN_REQUEST.java:19-21
void CM_GF_WEBSHOP_TOKEN_REQUEST::readImpl() {
}

// Java CM_GF_WEBSHOP_TOKEN_REQUEST.java:23-26
void CM_GF_WEBSHOP_TOKEN_REQUEST::runImpl() {
	sendPacket(serverpackets::SM_GF_WEBSHOP_TOKEN_RESPONSE("")); // TODO
}

AION_CLIENT_PACKET(CM_GF_WEBSHOP_TOKEN_REQUEST);

} // namespace aion::gameserver::network::aion::clientpackets
