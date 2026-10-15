#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/CreatureType.h"

namespace aion::gameserver::model {

/** Companion of the generated enum CreatureType (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
inline constexpr std::array<int32_t, 6> CREATURE_TYPE_IDS{{
	0,  // ATTACKABLE: regular monsters
	2,  // PEACE: Peace npc, which you cannot talk to
	8,  // AGGRESSIVE: monsters that are pre-aggressive
	10, // INVULNERABLE
	38, // FRIEND: non attackable NPCs, which you can talk to
	54, // SUPPORT
}};
static_assert(static_cast<size_t>(CreatureType::SUPPORT) + 1 == CREATURE_TYPE_IDS.size(), "one entry per CreatureType constant");
} // namespace detail

/** Java: CreatureType.getId() */
constexpr int32_t getId(CreatureType type) noexcept {
	return detail::CREATURE_TYPE_IDS[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::model
