#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/instance/InstanceProgressionType.h"

namespace aion::gameserver::model::instance {

/**
 * Companion of the generated enum InstanceProgressionType (static-data.md §2.5, LetterTypeInfo.h's pattern): Java's constructor data and
 * methods as free functions found by ADL (`getId(value)` for Java `value.getId()`).
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 4> INSTANCEPROGRESSIONTYPE_IDS{
	12 * 1024 * 1024, // REINFORCE_MEMBER, 12582912
	1 * 1024 * 1024, // PREPARING, 1048576
	2 * 1024 * 1024, // START_PROGRESS, 2097152
	3 * 1024 * 1024, // END_PROGRESS, 3145728
};
static_assert(static_cast<size_t>(InstanceProgressionType::END_PROGRESS) + 1 == INSTANCEPROGRESSIONTYPE_IDS.size(), "one entry per constant");
} // namespace detail

/** Java: getId() */
constexpr int32_t getId(InstanceProgressionType value) noexcept {
	return detail::INSTANCEPROGRESSIONTYPE_IDS[static_cast<size_t>(value)];
}

/** Java: isReinforcing() */
constexpr bool isReinforcing(InstanceProgressionType value) noexcept {
	return getId(value) == getId(InstanceProgressionType::REINFORCE_MEMBER);
}

/** Java: isPreparing() */
constexpr bool isPreparing(InstanceProgressionType value) noexcept {
	return getId(value) == getId(InstanceProgressionType::PREPARING);
}

/** Java: isStartProgress() */
constexpr bool isStartProgress(InstanceProgressionType value) noexcept {
	return getId(value) == getId(InstanceProgressionType::START_PROGRESS);
}

/** Java: isEndProgress() */
constexpr bool isEndProgress(InstanceProgressionType value) noexcept {
	return getId(value) == getId(InstanceProgressionType::END_PROGRESS);
}

} // namespace aion::gameserver::model::instance
