#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author synchro2
 */
class CraftCooldownsDAO {
public:
	static void loadCraftCooldowns(model::gameobjects::player::Player& player);
	static void storeCraftCooldowns(model::gameobjects::player::Player& player);
private:
	static void deleteCraftCoolDowns(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
