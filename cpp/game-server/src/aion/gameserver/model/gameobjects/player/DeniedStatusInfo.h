#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"

namespace aion::gameserver::model::gameobjects::player {

/** Companion of the generated enum DeniedStatus (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

/** Java: DeniedStatus.getId() - VIEW_DETAILS 1, TRADE 2, GROUP 4, GUILD 8, FRIEND 16, DUEL 32 (1 << ordinal) */
constexpr int32_t getId(DeniedStatus status) noexcept {
	return 1 << static_cast<int32_t>(status);
}

} // namespace aion::gameserver::model::gameobjects::player
