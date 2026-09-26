#include "aion/gameserver/network/aion/clientpackets/CM_MOTION.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_MOTION::CM_MOTION(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_MOTION::readImpl() {
	readC(); // unk 4
	motionId = readUH();
	motionType = readUC();
}

void CM_MOTION::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	player->getMotions().setActive(motionId, motionType);
}

AION_CLIENT_PACKET(CM_MOTION);

} // namespace aion::gameserver::network::aion::clientpackets
