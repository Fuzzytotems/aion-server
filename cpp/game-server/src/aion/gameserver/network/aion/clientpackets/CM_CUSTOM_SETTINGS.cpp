#include "aion/gameserver/network/aion/clientpackets/CM_CUSTOM_SETTINGS.h"

#include <memory>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CUSTOM_SETTINGS::CM_CUSTOM_SETTINGS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_CUSTOM_SETTINGS::readImpl() {
	display = readUH(); // see SM_CUSTOM_SETTINGS.HIDE_* variables
	/**
	 * 1 : view detail player 2 : trade 4 : party/force 8 : legion 16 : friend 32 : dual(pvp)
	 */
	deny = readUH();
}

void CM_CUSTOM_SETTINGS::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	activePlayer->getPlayerSettings()->setDisplay(display);
	activePlayer->getPlayerSettings()->setDeny(deny);

	utils::PacketSendUtility::broadcastPacket(*activePlayer, serverpackets::SM_CUSTOM_SETTINGS(*activePlayer), true);
}

AION_CLIENT_PACKET(CM_CUSTOM_SETTINGS);

} // namespace aion::gameserver::network::aion::clientpackets
