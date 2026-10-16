#include "aion/gameserver/network/aion/clientpackets/CM_STOP_TRAINING.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_STOP_TRAINING::CM_STOP_TRAINING(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_STOP_TRAINING.java:19-21: nothing to read
void CM_STOP_TRAINING::readImpl() {
}

// Java CM_STOP_TRAINING.java:24-27
void CM_STOP_TRAINING::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	player->getPosition()->getWorldMapInstance()->getInstanceHandler()->onStopTraining(*player);
}

AION_CLIENT_PACKET(CM_STOP_TRAINING);

} // namespace aion::gameserver::network::aion::clientpackets
