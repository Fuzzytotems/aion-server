#pragma once

#include <any>

#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/actions/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::actions {

/**
 * C++: a static-only class (fieldmap K5). setPlayerMode's `Object obj` is `const std::any&` (hub-headers.md §7.4): RIDE takes the
 * `const templates::ride::RideInfo*` template, IN_ROLL a `runtime::Ref<InRoll>`; a null object (Java: null) is an empty std::any or a null
 * pointer/Ref of that type. Any other content throws std::bad_any_cast (Java: ClassCastException).
 *
 * @author xTz
 */
class PlayerActions {
public:
	PlayerActions() = delete;

	static bool isInPlayerMode(gameobjects::player::Player& player, PlayerMode mode);

	static void setPlayerMode(gameobjects::player::Player& player, PlayerMode mode, const std::any& obj);

	static bool unsetPlayerMode(gameobjects::player::Player& player, PlayerMode mode);
};

} // namespace aion::gameserver::model::actions
