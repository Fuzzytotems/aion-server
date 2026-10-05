#pragma once

#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/TeamType.h"

namespace aion::gameserver::model::team {

/** Companion of the generated enum TeamType (docs/design/static-data.md §2.5): Java's constructor data and methods as constexpr free functions. */

/** Java: TeamType.getType() - GROUP 0x3F, AUTO_GROUP 0x02, ALLIANCE 0x3F, AUTO_ALLIANCE 0x36, ALLIANCE_DEFENCE 0x3F, ALLIANCE_OFFENCE 0x02 */
constexpr int32_t getType(TeamType type) noexcept {
	constexpr int32_t TYPES[] = {0x3F, 0x02, 0x3F, 0x36, 0x3F, 0x02};
	return TYPES[static_cast<size_t>(type)];
}

/** Java: TeamType.getSubType() - 0, 1, 0, 1, 4, 3 */
constexpr int32_t getSubType(TeamType type) noexcept {
	constexpr int32_t SUB_TYPES[] = {0, 1, 0, 1, 4, 3};
	return SUB_TYPES[static_cast<size_t>(type)];
}

/** Java: TeamType.isAutoTeam() */
constexpr bool isAutoTeam(TeamType type) noexcept {
	return getType(type) == 0x02;
}

/** Java: TeamType.isOffence() */
constexpr bool isOffence(TeamType type) noexcept {
	return getSubType(type) == 3;
}

/** Java: TeamType.isDefence() */
constexpr bool isDefence(TeamType type) noexcept {
	return getSubType(type) == 4;
}

} // namespace aion::gameserver::model::team
