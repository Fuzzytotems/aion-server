#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_SET_MEMO.h"

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

CM_FRIEND_SET_MEMO::CM_FRIEND_SET_MEMO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_FRIEND_SET_MEMO.java:25-28
void CM_FRIEND_SET_MEMO::readImpl() {
	targetName = readS();
	memo = readS();
}

// Java CM_FRIEND_SET_MEMO.java:31-39
void CM_FRIEND_SET_MEMO::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<model::gameobjects::player::Friend> friend_ = activePlayer->getFriendList().getFriend(targetName);
	if (friend_ == nullptr) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NOT_IN_LIST());
	} else {
		services::SocialService::setFriendMemo(*activePlayer, *friend_, memo);
	}
}

AION_CLIENT_PACKET(CM_FRIEND_SET_MEMO);

} // namespace aion::gameserver::network::aion::clientpackets
