#include "aion/gameserver/network/aion/clientpackets/CM_SET_NOTE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_NOTE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Friend;
using model::gameobjects::player::Player;
using utils::PacketSendUtility;

CM_SET_NOTE::CM_SET_NOTE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SET_NOTE.java:28-30
void CM_SET_NOTE::readImpl() {
	note = readS();
}

// Java CM_SET_NOTE.java:33-44
void CM_SET_NOTE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (note == player->getCommonData()->getNote())
		return;
	player->getCommonData()->setNote(note);
	for (const runtime::Ptr<Friend>& friend_ : player->getFriendList()) {
		if (const runtime::Ptr<Player> friendPlayer = world::World::getInstance().getPlayer(friend_->getObjectId()))
			PacketSendUtility::sendPacket(*friendPlayer, serverpackets::SM_FRIEND_LIST());
	}
	PacketSendUtility::broadcastPacketAndReceive(*player, serverpackets::SM_UPDATE_NOTE(*player));
}

AION_CLIENT_PACKET(CM_SET_NOTE);

} // namespace aion::gameserver::network::aion::clientpackets
