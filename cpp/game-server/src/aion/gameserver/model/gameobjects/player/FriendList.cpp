#include "aion/gameserver/model/gameobjects/player/FriendList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::gameobjects::player {

FriendList::FriendList(Player& owner, const std::vector<runtime::Ptr<Friend>>& friendsValue) : OwnedPart(owner), player(owner) {
	for (const runtime::Ptr<Friend>& friend_ : friendsValue)
		friends.put(friend_->getObjectId(), runtime::Ref<Friend>(*friend_));
}

FriendList::~FriendList() = default;

runtime::Ptr<Friend> FriendList::getFriend(int32_t objId) {
	AION_UNPORTED();
}

int32_t FriendList::getSize() {
	AION_UNPORTED();
}

void FriendList::addFriend(Friend& friend_) {
	AION_UNPORTED();
}

runtime::Ptr<Friend> FriendList::getFriend(std::string_view name) {
	AION_UNPORTED();
}

void FriendList::delFriend(int32_t friendOid) {
	AION_UNPORTED();
}

bool FriendList::isFull() {
	AION_UNPORTED();
}

void FriendList::setStatus(Status value, PlayerCommonData& pcd) {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::Ptr<Friend>> FriendList::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::Ptr<Friend>> FriendList::begin() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
