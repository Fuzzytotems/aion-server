#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aion/gameserver/model/summons/SummonMode.h"

namespace aion::gameserver::model::summons {

/** Companion of the generated enum SummonMode (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the id constructor argument, in ordinal order (SummonMode.java:9-13) */
inline constexpr std::array<int32_t, 5> SUMMON_MODE_IDS{{
	0, // ATTACK
	1, // GUARD
	2, // REST
	3, // RELEASE
	5, // UNK
}};
static_assert(static_cast<size_t>(SummonMode::UNK) + 1 == SUMMON_MODE_IDS.size(), "one entry per SummonMode constant");
} // namespace detail

/** Java: SummonMode.getId() */
constexpr int32_t getId(SummonMode mode) noexcept {
	return detail::SUMMON_MODE_IDS[static_cast<size_t>(mode)];
}

/** Java: SummonMode.getSummonModeById(int) - the constant with the id, null (empty) if there is none */
constexpr std::optional<SummonMode> getSummonModeById(int32_t id) noexcept {
	for (size_t ordinal = 0; ordinal < detail::SUMMON_MODE_IDS.size(); ++ordinal)
		if (detail::SUMMON_MODE_IDS[ordinal] == id)
			return static_cast<SummonMode>(ordinal);
	return std::nullopt;
}

} // namespace aion::gameserver::model::summons
