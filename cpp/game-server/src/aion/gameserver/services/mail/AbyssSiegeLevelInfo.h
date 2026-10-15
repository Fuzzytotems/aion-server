#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/services/mail/AbyssSiegeLevel.h"

namespace aion::gameserver::services::mail {

/** Companion of the generated enum AbyssSiegeLevel (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the value constructor argument, in ordinal order (AbyssSiegeLevel.java:9-13) */
inline constexpr std::array<int32_t, 5> ABYSS_SIEGE_LEVEL_IDS{{
	0, // NONE
	1, // HERO_DECORATION
	2, // MEDAL
	3, // ELITE_SOLDIER
	4, // VETERAN_SOLDIER
}};
static_assert(static_cast<size_t>(AbyssSiegeLevel::VETERAN_SOLDIER) + 1 == ABYSS_SIEGE_LEVEL_IDS.size(), "one entry per AbyssSiegeLevel constant");
} // namespace detail

/** Java: AbyssSiegeLevel.getId() */
constexpr int32_t getId(AbyssSiegeLevel level) noexcept {
	return detail::ABYSS_SIEGE_LEVEL_IDS[static_cast<size_t>(level)];
}

/**
 * Java: AbyssSiegeLevel.getLevelById(int) (AbyssSiegeLevel.java:23-30).
 * @throws IllegalArgumentException "There is no AbyssSiegeLevel with ID <id>" for an unknown id
 */
inline AbyssSiegeLevel getLevelById(int32_t id) {
	for (size_t ordinal = 0; ordinal < detail::ABYSS_SIEGE_LEVEL_IDS.size(); ++ordinal) {
		if (detail::ABYSS_SIEGE_LEVEL_IDS[ordinal] == id)
			return static_cast<AbyssSiegeLevel>(ordinal);
	}
	throw commons::utils::IllegalArgumentException("There is no AbyssSiegeLevel with ID " + std::to_string(id));
}

} // namespace aion::gameserver::services::mail
