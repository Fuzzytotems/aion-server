#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/player/FriendList_Status.h"

namespace aion::gameserver::model::gameobjects::player {

/** Companion of the generated nested enum FriendList.Status (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

/** Java: Status.getId() - OFFLINE 0 (offline or invisible), ONLINE 1, AWAY 3 (away or busy) */
constexpr int8_t getId(FriendList_Status status) noexcept {
	switch (status) {
		case FriendList_Status::OFFLINE:
			return 0;
		case FriendList_Status::ONLINE:
			return 1;
		case FriendList_Status::AWAY:
			break;
	}
	return 3;
}

/** Java: Status.getByValue(value): the status with the id, null (std::nullopt) if there is none */
constexpr std::optional<FriendList_Status> getByValue(int8_t value) noexcept {
	for (FriendList_Status status : {FriendList_Status::OFFLINE, FriendList_Status::ONLINE, FriendList_Status::AWAY}) {
		if (getId(status) == value)
			return status;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::gameobjects::player
