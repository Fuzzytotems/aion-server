#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_DEL.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/services/SocialService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_FRIEND_DEL::CM_FRIEND_DEL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_FRIEND_DEL.java:24-26
void CM_FRIEND_DEL::readImpl() {
	targetName = readS();
}

// Java CM_FRIEND_DEL.java:29-37
void CM_FRIEND_DEL::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::player::Friend> friend_ = activePlayer->getFriendList().getFriend(targetName);
	if (friend_ == nullptr) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NOT_IN_LIST());
	} else {
		services::SocialService::deleteFriend(*activePlayer, *friend_);
	}
}

AION_CLIENT_PACKET(CM_FRIEND_DEL);

} // namespace aion::gameserver::network::aion::clientpackets
