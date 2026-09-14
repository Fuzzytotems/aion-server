#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::services::toypet {

/**
 * @author ATracer
 */
class PetSpawnService {
public:
	static void summonPet(model::gameobjects::player::Player& player, int32_t templateId);
};

} // namespace aion::gameserver::services::toypet
