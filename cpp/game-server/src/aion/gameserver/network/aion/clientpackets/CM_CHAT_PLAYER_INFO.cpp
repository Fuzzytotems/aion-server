#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_PLAYER_INFO.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_CHAT_PLAYER_INFO::CM_CHAT_PLAYER_INFO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHAT_PLAYER_INFO.java:25-27
void CM_CHAT_PLAYER_INFO::readImpl() {
	playerName = readS();
}

// Java CM_CHAT_PLAYER_INFO.java:30-38
void CM_CHAT_PLAYER_INFO::runImpl() {
	const runtime::Ptr<Player> target = world::World::getInstance().getPlayer(utils::ChatUtil::getRealCharName(playerName));
	if (target == nullptr) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}
	if (!getConnection()->getActivePlayer()->getKnownList().knows(*target))
		sendPacket(serverpackets::SM_CHAT_WINDOW(*target, false));
}

AION_CLIENT_PACKET(CM_CHAT_PLAYER_INFO);

} // namespace aion::gameserver::network::aion::clientpackets
