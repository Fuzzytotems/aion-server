#include "aion/gameserver/network/aion/clientpackets/CM_OPEN_STATICDOOR.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/StaticDoorService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_OPEN_STATICDOOR::CM_OPEN_STATICDOOR(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_OPEN_STATICDOOR.java:26-29
void CM_OPEN_STATICDOOR::readImpl() {
	doorId = readD();
}

// Java CM_OPEN_STATICDOOR.java:31-35
void CM_OPEN_STATICDOOR::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = this->getConnection()->getActivePlayer();
	services::StaticDoorService::getInstance().openStaticDoor(*player, doorId);
}

AION_CLIENT_PACKET(CM_OPEN_STATICDOOR);

} // namespace aion::gameserver::network::aion::clientpackets
