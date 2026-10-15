#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/abyss/AbyssSkills.h"
#include "aion/gameserver/services/abyss/fwd.h"

namespace aion::gameserver::services::abyss {

/**
 * C++: a static-only class (hub-headers.md §11.1). The package-private enum AbyssSkills of the same Java file is generated (AbyssSkills.h).
 *
 * @author ATracer
 */
class AbyssSkillService {
public:
	static void updateSkills(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::abyss
