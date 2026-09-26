#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"

namespace aion::gameserver::model::gameobjects::player {

BlockedPlayer::BlockedPlayer(int32_t objIdValue, std::string_view nameValue, std::string_view reasonValue)
	: objId(objIdValue), name(std::string(nameValue)), reason(std::string(reasonValue)) {
}

BlockedPlayer::~BlockedPlayer() = default;

runtime::Ref<BlockedPlayer> BlockedPlayer::create(int32_t objIdValue, std::string_view nameValue, std::string_view reasonValue) {
	return runtime::makeRef<BlockedPlayer>(objIdValue, nameValue, reasonValue);
}

std::string BlockedPlayer::getReason() {
	SYNCHRONIZED(*this) {
		return reason.get();
	}
}

void BlockedPlayer::setReason(std::string_view value) {
	SYNCHRONIZED(*this) {
		reason.set(std::string(value));
	}
}

} // namespace aion::gameserver::model::gameobjects::player
