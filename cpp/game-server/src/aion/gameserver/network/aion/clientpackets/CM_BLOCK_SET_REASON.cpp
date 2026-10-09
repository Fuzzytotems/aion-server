#include "aion/gameserver/network/aion/clientpackets/CM_BLOCK_SET_REASON.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/services/SocialService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BLOCK_SET_REASON::CM_BLOCK_SET_REASON(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BLOCK_SET_REASON.java:25-29
void CM_BLOCK_SET_REASON::readImpl() {
	targetName = readS();
	reason = readS();
}

// Java CM_BLOCK_SET_REASON.java:32-41
void CM_BLOCK_SET_REASON::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::player::BlockedPlayer> target = activePlayer->getBlockList()->getBlockedPlayer(targetName);
	if (target == nullptr)
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_BLOCKLIST_NOT_IN_LIST());
	else {
		services::SocialService::setBlockedReason(*activePlayer, *target, reason);
	}
}

AION_CLIENT_PACKET(CM_BLOCK_SET_REASON);

} // namespace aion::gameserver::network::aion::clientpackets
