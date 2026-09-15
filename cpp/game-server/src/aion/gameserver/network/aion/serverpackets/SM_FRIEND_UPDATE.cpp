#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_UPDATE.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/FriendList_StatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.serverpackets.SM_FRIEND_UPDATE");

SM_FRIEND_UPDATE::SM_FRIEND_UPDATE(int32_t friendObjIdValue) : AionServerPacket(opcodeOf<SM_FRIEND_UPDATE>), friendObjId(friendObjIdValue) {
}

void SM_FRIEND_UPDATE::writeImpl(AionConnection* con) {
	using model::gameobjects::player::FriendList_Status;
	if (con == nullptr)
		throw runtime::NullPointerException("SM_FRIEND_UPDATE::writeImpl without a connection");
	runtime::Ptr<model::gameobjects::player::Friend> f = con->getActivePlayer()->getFriendList().getFriend(friendObjId);
	if (!f)
		log.debug("Attempted to update friend list status of " + std::to_string(friendObjId) + " for " + con->getActivePlayer()->getName() +
			" - object ID not found on friend list");
	else {
		writeS(f->getName());
		writeD(f->getLevel());
		writeD(model::getClassId(f->getPlayerClass()));
		writeC(model::getGenderId(f->getGender()));
		writeD(f->getMapId());
		writeD(f->getStatus() == FriendList_Status::ONLINE ? 0 : f->getLastOnlineEpochSeconds());
		writeS(f->getNote());
		writeC(model::gameobjects::player::getId(f->getStatus()));
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
