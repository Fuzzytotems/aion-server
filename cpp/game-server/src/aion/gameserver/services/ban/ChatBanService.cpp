#include "aion/gameserver/services/ban/ChatBanService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::ban {

void ChatBanService::banPlayer(model::gameobjects::player::Player& player, int64_t durationMillis) {
	AION_UNPORTED();
}

void ChatBanService::unbanPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// anonymous Runnable at ChatBanService.java:40 (fieldmap key ChatBanService$1); argument 1 of schedule(); storage: task
void ChatBanService::registerUnban(model::gameobjects::player::Player& player, int64_t delay) {
	AION_UNPORTED();
}

bool ChatBanService::isBanned(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t ChatBanService::getBanMinutes(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::ban
