#include "aion/gameserver/network/aion/clientpackets/CM_CLIENT_COMMAND_ROLL.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

CM_CLIENT_COMMAND_ROLL::CM_CLIENT_COMMAND_ROLL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CLIENT_COMMAND_ROLL.java:26-28
void CM_CLIENT_COMMAND_ROLL::readImpl() {
	maxRoll = readD();
}

// Java CM_CLIENT_COMMAND_ROLL.java:31-38
void CM_CLIENT_COMMAND_ROLL::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (maxRoll <= 0) // client sends 100 on /roll 0 but negative numbers are passed through for whatever reason
		maxRoll = 100;
	const int32_t roll = commons::utils::Rnd::get(1, maxRoll);
	PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_DICE_CUSTOM_ME(roll, maxRoll));
	PacketSendUtility::broadcastPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_DICE_CUSTOM_OTHER(player->getName(), roll, maxRoll));
}

AION_CLIENT_PACKET(CM_CLIENT_COMMAND_ROLL);

} // namespace aion::gameserver::network::aion::clientpackets
