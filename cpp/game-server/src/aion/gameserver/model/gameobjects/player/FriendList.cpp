#include "aion/gameserver/model/gameobjects/player/FriendList.h"

#include <memory>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_NOTIFY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_UPDATE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::gameobjects::player {

FriendList::FriendList(Player& owner, const std::vector<runtime::Ptr<Friend>>& friendsValue) : OwnedPart(owner), player(owner) {
	for (const runtime::Ptr<Friend>& friend_ : friendsValue)
		friends.put(friend_->getObjectId(), runtime::Ref<Friend>(*friend_));
}

FriendList::~FriendList() = default;

runtime::Ptr<Friend> FriendList::getFriend(int32_t objId) {
	return friends.get(objId);
}

int32_t FriendList::getSize() {
	return friends.size();
}

void FriendList::addFriend(Friend& friend_) {
	friends.put(friend_.getObjectId(), runtime::Ref<Friend>(friend_));
}

runtime::Ptr<Friend> FriendList::getFriend(std::string_view name) {
	for (const runtime::Ptr<Friend>& friend_ : friends.values()) {
		if (commons::utils::StringUtils::equalsIgnoreCase(friend_->getName(), name))
			return friend_;
	}
	return nullptr;
}

void FriendList::delFriend(int32_t friendOid) {
	friends.remove(friendOid);
}

bool FriendList::isFull() {
	return getSize() >= configs::main::CustomConfig::FRIENDLIST_SIZE.load();
}

void FriendList::setStatus(Status value, PlayerCommonData& pcd) {
	Status previousStatus = status.get();
	status.set(value);

	for (const runtime::Ptr<Friend>& friend_ : friends.values()) {
		runtime::Ptr<Player> friendPlayer = world::World::getInstance().getPlayer(friend_->getObjectId());
		if (!friendPlayer)
			continue;

		friendPlayer->getFriendList().getFriend(pcd.getPlayerObjId())->setPCD(runtime::Ptr<PlayerCommonData>(pcd));
		utils::PacketSendUtility::sendPacket(*friendPlayer, network::aion::serverpackets::SM_FRIEND_UPDATE(player.getObjectId()));

		if (previousStatus == Status::OFFLINE) {
			// Show LOGIN message
			utils::PacketSendUtility::sendPacket(*friendPlayer,
				network::aion::serverpackets::SM_FRIEND_NOTIFY(network::aion::serverpackets::SM_FRIEND_NOTIFY::LOGIN, player.getName()));
		} else if (value == Status::OFFLINE) {
			// Show LOGOUT message
			utils::PacketSendUtility::sendPacket(*friendPlayer,
				network::aion::serverpackets::SM_FRIEND_NOTIFY(network::aion::serverpackets::SM_FRIEND_NOTIFY::LOGOUT, player.getName()));
		}
	}
}

runtime::JavaIterator<runtime::Ptr<Friend>> FriendList::iterator() {
	return friends.values().iterator();
}

runtime::SnapshotIterator<runtime::Ptr<Friend>> FriendList::begin() {
	return runtime::SnapshotIterator<runtime::Ptr<Friend>>(std::make_shared<const std::vector<runtime::Ptr<Friend>>>(friends.values().toVector()));
}

} // namespace aion::gameserver::model::gameobjects::player
