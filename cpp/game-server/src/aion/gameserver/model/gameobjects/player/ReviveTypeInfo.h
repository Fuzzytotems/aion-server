#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/model/gameobjects/player/ReviveType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::player {

/** Companion of the generated enum ReviveType (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions (ADL). */

namespace detail {
/** Java ReviveType type ids in ordinal order */
inline constexpr std::array<int32_t, 7> REVIVE_TYPE_IDS{
	0, // BIND_REVIVE: revive to bindpoint
	1, // REBIRTH_REVIVE: revive from rebirth effect
	2, // ITEM_SELF_REVIVE: self-rez stone
	3, // SKILL_REVIVE: revive from skill
	4, // KISK_REVIVE: revive to kisk
	6, // INSTANCE_REVIVE: revive to instance start point
	8, // OBELISK_REVIVE: revive to obelisk
};
static_assert(static_cast<size_t>(ReviveType::OBELISK_REVIVE) + 1 == REVIVE_TYPE_IDS.size());
} // namespace detail

/** Java: ReviveType.getReviveTypeId() */
constexpr int32_t getReviveTypeId(ReviveType type) noexcept {
	return detail::REVIVE_TYPE_IDS[static_cast<size_t>(type)];
}

/** Java: ReviveType.getReviveTypeById(id). @throws IllegalArgumentException("Unsupported revive type: " + id) */
inline ReviveType getReviveTypeById(int32_t id) {
	for (size_t i = 0; i < detail::REVIVE_TYPE_IDS.size(); ++i) {
		if (detail::REVIVE_TYPE_IDS[i] == id)
			return static_cast<ReviveType>(i);
	}
	throw runtime::IllegalArgumentException("Unsupported revive type: " + std::to_string(id));
}

} // namespace aion::gameserver::model::gameobjects::player
