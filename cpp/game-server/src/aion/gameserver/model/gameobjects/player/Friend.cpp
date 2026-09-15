#include "aion/gameserver/model/gameobjects/player/Friend.h"

#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::model::gameobjects::player {

Friend::Friend(PlayerCommonData& pcdValue, std::string_view memoValue) : memo(std::string(memoValue)) {
	pcd.set(runtime::Ref<PlayerCommonData>(pcdValue));
}

Friend::~Friend() = default;

runtime::Ref<Friend> Friend::create(PlayerCommonData& pcdValue, std::string_view memoValue) {
	return runtime::makeRef<Friend>(pcdValue, memoValue);
}

FriendList_Status Friend::getStatus() {
	if (!pcd->isOnline())
		return FriendList::Status::OFFLINE;
	runtime::Ptr<Player> player = world::World::getInstance().getPlayer(getObjectId());
	if (!player)
		return FriendList::Status::OFFLINE;
	return player->getFriendList().getStatus();
}

void Friend::setPCD(runtime::Ptr<PlayerCommonData> value) {
	pcd.set(value);
}

std::string Friend::getName() {
	return pcd->getName();
}

int32_t Friend::getLevel() {
	return pcd->getLevel();
}

std::string Friend::getNote() {
	return pcd->getNote();
}

PlayerClass Friend::getPlayerClass() {
	return pcd->getPlayerClass();
}

Gender Friend::getGender() {
	return pcd->getGender();
}

int32_t Friend::getMapId() {
	return pcd->getMapId();
}

int32_t Friend::getLastOnlineEpochSeconds() {
	return pcd->getLastOnlineEpochSeconds();
}

int32_t Friend::getObjectId() {
	return pcd->getPlayerObjId();
}

std::string Friend::getFriendMemo() {
	SYNCHRONIZED(*this) {
		return memo.get();
	}
}

void Friend::setFriendMemo(std::string_view value) {
	SYNCHRONIZED(*this) {
		memo.set(std::string(value));
	}
}

} // namespace aion::gameserver::model::gameobjects::player
