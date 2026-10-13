#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/instance/InstanceScoreType.h"

namespace aion::gameserver::model::instance {

/**
 * Companion of the generated enum InstanceScoreType (static-data.md §2.5, LetterTypeInfo.h's pattern): Java's constructor data as a free
 * function found by ADL (`getId(type)` for Java `type.getId()`).
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 9> INSTANCESCORETYPE_IDS{
	2, // UPDATE_INSTANCE_PROGRESS
	3, // INIT_PLAYER
	4, // UPDATE_PLAYER_BUFF_STATUS
	5, // SHOW_REWARD
	6, // UPDATE_INSTANCE_BUFFS_AND_SCORE
	7, // UPDATE_ALL_PLAYER_INFO
	8, // PLAYER_QUIT
	10, // UPDATE_RANK
	11, // UPDATE_FACTION_SCORE
};
static_assert(static_cast<size_t>(InstanceScoreType::UPDATE_FACTION_SCORE) + 1 == INSTANCESCORETYPE_IDS.size(), "one entry per constant");
} // namespace detail

/** Java: getId() */
constexpr int32_t getId(InstanceScoreType value) noexcept {
	return detail::INSTANCESCORETYPE_IDS[static_cast<size_t>(value)];
}

} // namespace aion::gameserver::model::instance
