#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/services/antihack/fwd.h"

namespace aion::gameserver::services::antihack {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author Source
 */
class AntiHackService {
public:
	static bool canMove(model::gameobjects::player::Player& player, float x, float y, float z, int8_t type);
private:
	static bool punish(model::gameobjects::player::Player& player, bool normalMovePacket, std::string_view message);
	static void moveBack(model::gameobjects::player::Player& player, bool normalMovePacket);
public:
	static void checkAionBin(int32_t size, network::aion::AionConnection* con);
};

} // namespace aion::gameserver::services::antihack
