#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * Class that is responsible for loading/storing player appearance
 *
 * @author SoulKeeper, AEJTester, srx47
 */
class PlayerAppearanceDAO {
public:
	static runtime::Ref<model::gameobjects::player::PlayerAppearance> load(int32_t playerId);
	/**
	 * Saves player appearance in database.<br>
	 * Actually calls {@link #store(int, com.aionemu.gameserver.model.gameobjects.player.PlayerAppearance)}
	 */
	static bool store(model::gameobjects::player::Player& player);
	static bool store(int32_t id, model::gameobjects::player::PlayerAppearance& pa);
};

} // namespace aion::gameserver::dao
