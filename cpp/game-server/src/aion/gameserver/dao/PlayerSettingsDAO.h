#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ATracer, Neon
 */
class PlayerSettingsDAO {
public:
	/**
	 * TODO 1) analyze possibility to zip settings 2) insert/update instead of replace 0 - uisettings 1 - shortcuts 2 - display 3 - deny
	 */
	static runtime::Ref<model::gameobjects::player::PlayerSettings> loadSettings(int32_t playerId);
	static void saveSettings(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
