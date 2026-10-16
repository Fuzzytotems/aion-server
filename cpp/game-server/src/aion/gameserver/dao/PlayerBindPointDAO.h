#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author evilset
 */
class PlayerBindPointDAO {
public:
	static void loadBindPoint(model::gameobjects::player::Player& player);
	static bool insertBindPoint(model::gameobjects::player::Player& player);
	static bool updateBindPoint(model::gameobjects::player::Player& player);
	static bool store(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
