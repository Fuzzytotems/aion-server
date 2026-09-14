#include "aion/gameserver/services/player/PlayerLeaveWorldService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::player {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.player.PlayerLeaveWorldService@L53:71

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.PlayerLeaveWorldService");

void PlayerLeaveWorldService::leaveWorldDelayed(model::gameobjects::player::Player& player, int64_t delayInMillis) {
	AION_UNPORTED();
}

void PlayerLeaveWorldService::leaveWorld(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::player
