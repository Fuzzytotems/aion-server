#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/skillengine/model/EffectReserved_ResourceType.h"

namespace aion::gameserver::skillengine::model {

/**
 * Companion of the generated enum EffectReserved.ResourceType (docs/design/static-data.md §2.5): Java's constructor data (ADL). `of(HealType)`
 * keeps its stand-in in AbstractHealEffect.cpp.
 */

namespace detail {
/** Java: the value constructor argument, in ordinal order (EffectReserved.java, enum ResourceType: HP(0), MP(1), FP(2), DP(3)) */
inline constexpr std::array<int32_t, 4> RESOURCE_TYPE_VALUES{{
	0, // HP
	1, // MP
	2, // FP
	3, // DP - TODO recheck (Java's comment)
}};
static_assert(static_cast<size_t>(EffectReserved_ResourceType::DP) + 1 == RESOURCE_TYPE_VALUES.size(), "one entry per ResourceType constant");
} // namespace detail

/** Java: EffectReserved.ResourceType.getValue() */
constexpr int32_t getValue(EffectReserved_ResourceType resourceType) noexcept {
	return detail::RESOURCE_TYPE_VALUES[static_cast<size_t>(resourceType)];
}

} // namespace aion::gameserver::skillengine::model
