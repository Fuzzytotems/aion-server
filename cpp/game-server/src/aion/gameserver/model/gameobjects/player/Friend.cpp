#include "aion/gameserver/model/gameobjects/player/Friend.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

namespace aion::gameserver::model::gameobjects::player {

Friend::Friend(PlayerCommonData& pcdValue, std::string_view memoValue) : memo(std::string(memoValue)) {
	pcd.set(runtime::Ref<PlayerCommonData>(pcdValue));
}

Friend::~Friend() = default;

runtime::Ref<Friend> Friend::create(PlayerCommonData& pcdValue, std::string_view memoValue) {
	return runtime::makeRef<Friend>(pcdValue, memoValue);
}

FriendList_Status Friend::getStatus() {
	AION_UNPORTED();
}

void Friend::setPCD(runtime::Ptr<PlayerCommonData> value) {
	pcd.set(value);
}

std::string Friend::getName() {
	AION_UNPORTED();
}

int32_t Friend::getLevel() {
	AION_UNPORTED();
}

std::string Friend::getNote() {
	AION_UNPORTED();
}

PlayerClass Friend::getPlayerClass() {
	AION_UNPORTED();
}

Gender Friend::getGender() {
	AION_UNPORTED();
}

int32_t Friend::getMapId() {
	AION_UNPORTED();
}

int32_t Friend::getLastOnlineEpochSeconds() {
	AION_UNPORTED();
}

int32_t Friend::getObjectId() {
	AION_UNPORTED();
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
