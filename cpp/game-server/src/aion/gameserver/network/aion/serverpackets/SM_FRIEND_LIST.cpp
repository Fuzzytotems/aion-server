#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"

#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/FriendList_StatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::serverpackets {

SM_FRIEND_LIST::SM_FRIEND_LIST() : AionServerPacket(opcodeOf<SM_FRIEND_LIST>) {
}

void SM_FRIEND_LIST::writeImpl(AionConnection* con) {
	using model::gameobjects::player::FriendList_Status;
	if (con == nullptr)
		throw runtime::NullPointerException("SM_FRIEND_LIST::writeImpl without a connection");
	model::gameobjects::player::FriendList& list = con->getActivePlayer()->getFriendList();
	writeH(-list.getSize());
	writeC(0); // unk
	for (runtime::Ptr<model::gameobjects::player::Friend> friend_ : list) {
		writeD(friend_->getObjectId());
		writeS(friend_->getName());
		writeD(friend_->getLevel());
		writeD(model::getClassId(friend_->getPlayerClass()));
		writeC(model::getGenderId(friend_->getGender()));
		writeD(friend_->getMapId());
		writeD(friend_->getStatus() == FriendList_Status::ONLINE ? 0 : friend_->getLastOnlineEpochSeconds());
		writeS(friend_->getNote()); // Friend note
		writeC(model::gameobjects::player::getId(friend_->getStatus()));
		runtime::Ptr<model::house::House> house = services::HousingService::getInstance().findActiveHouse(friend_->getObjectId());
		writeD(!house ? 0 : house->getAddress()->getId());
		writeC(!house ? 0 : detail::houseDoorStateId(house->getDoorState()));
		writeS(friend_->getFriendMemo());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
