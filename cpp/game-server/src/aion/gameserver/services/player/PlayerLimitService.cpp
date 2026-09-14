#include "aion/gameserver/services/player/PlayerLimitService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::player {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.player.PlayerLimitService@L52:38

int64_t PlayerLimitService::updateSellLimit(model::gameobjects::player::Player& player, int64_t itemPrice, int64_t itemCount) {
	AION_UNPORTED();
}

void PlayerLimitService::scheduleUpdate() {
	AION_UNPORTED();
}

PlayerLimitService& PlayerLimitService::getInstance() {
	static PlayerLimitService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services::player
