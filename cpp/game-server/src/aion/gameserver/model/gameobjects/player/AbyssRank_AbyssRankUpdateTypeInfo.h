#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/AbyssRank_AbyssRankUpdateType.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Companion of the generated nested enum AbyssRank.AbyssRankUpdateType (docs/design/static-data.md §2.5): Java's constructor data as constexpr
 * free functions (ADL).
 */

/** Java: AbyssRankUpdateType.value() - PLAYER_ELYOS 1, PLAYER_ASMODIANS 2, LEGION_ELYOS 4, LEGION_ASMODIANS 8 (1 << ordinal) */
constexpr int32_t value(AbyssRank_AbyssRankUpdateType type) noexcept {
	return 1 << static_cast<int32_t>(type);
}

} // namespace aion::gameserver::model::gameobjects::player
