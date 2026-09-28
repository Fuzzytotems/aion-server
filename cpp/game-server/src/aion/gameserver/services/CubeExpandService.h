#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer, Simple, Luzien
 */
class CubeExpandService {
public:
	/** Shows Question window and expands on positive response */
	static void expandCube(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
private:
	/** Expands the cubes */
	static void expand(model::gameobjects::player::Player& player, int32_t type);
public:
	static void questExpand(model::gameobjects::player::Player& player);
	static void itemExpand(model::gameobjects::player::Player& player);
	static void npcExpand(model::gameobjects::player::Player& player);
	static bool canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel);
	static bool canExpand(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services
