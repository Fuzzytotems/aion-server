#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/instance/fwd.h"

namespace aion::gameserver::services::instance {

/**
 * @author xTz
 */
class PvPArenaService {
public:
	static bool isPvPArenaAvailable(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt);
	static bool checkItem(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt);
private:
	static bool checkTime(model::autogroup::AutoGroupType agt);
	static bool isPvPArenaAvailable();
	static bool isHarmonyArenaAvailable();
	static bool isGloryArenaAvailable();
};

} // namespace aion::gameserver::services::instance
