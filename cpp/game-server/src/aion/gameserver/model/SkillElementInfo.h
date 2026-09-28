#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"

namespace aion::gameserver::model {

/** Companion of the generated enum SkillElement (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
using stats::container::StatEnum;

/** Java constructor argument statEnum in ordinal order (null for NONE) */
inline constexpr std::array<std::optional<StatEnum>, 7> SKILL_ELEMENT_STATS{{
	std::nullopt,                // NONE
	StatEnum::FIRE_RESISTANCE,   // FIRE
	StatEnum::WATER_RESISTANCE,  // WATER
	StatEnum::WIND_RESISTANCE,   // WIND
	StatEnum::EARTH_RESISTANCE,  // EARTH
	StatEnum::LIGHT_RESISTANCE,  // LIGHT
	StatEnum::DARK_RESISTANCE,   // DARK
}};
static_assert(static_cast<size_t>(SkillElement::DARK) + 1 == SKILL_ELEMENT_STATS.size(), "one entry per SkillElement constant");
} // namespace detail

/** Java: SkillElement.getStatForElement() - null (std::nullopt) for NONE */
constexpr std::optional<stats::container::StatEnum> getStatForElement(SkillElement element) noexcept {
	return detail::SKILL_ELEMENT_STATS[static_cast<size_t>(element)];
}

} // namespace aion::gameserver::model
