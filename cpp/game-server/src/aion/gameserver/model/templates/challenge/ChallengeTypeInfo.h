#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/model/templates/challenge/ChallengeType.h"
#include "aion/gameserver/model/templates/detail/EnumValueOf.h"

namespace aion::gameserver::model::templates::challenge {

/**
 * Companion of the generated enum ChallengeType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods
 * as free functions found by ADL (`getId(type)` for Java `type.getId()`). The static `ChallengeType.fromValue(name)` is
 * `challenge::fromValue<ChallengeType>(name)`, since RewardType declares a fromValue too.
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 2> CHALLENGE_TYPE_IDS{
	1, // LEGION
	2, // TOWN
};
static_assert(static_cast<size_t>(ChallengeType::TOWN) + 1 == CHALLENGE_TYPE_IDS.size(), "one entry per ChallengeType constant");
} // namespace detail

constexpr int32_t getId(ChallengeType type) noexcept {
	return detail::CHALLENGE_TYPE_IDS[static_cast<size_t>(type)];
}

/** Java value(): name() */
constexpr std::string_view value(ChallengeType type) noexcept {
	return xml::enumName(type);
}

/** The static fromValue(String) of the challenge enums (specialized per enum). @throws IllegalArgumentException for an unknown name */
template <class E>
E fromValue(std::string_view paramString);

/** Java static ChallengeType.fromValue(paramString): valueOf(paramString) */
template <>
inline ChallengeType fromValue<ChallengeType>(std::string_view paramString) {
	return templates::detail::enumValueOf<ChallengeType>(paramString, "com.aionemu.gameserver.model.templates.challenge.ChallengeType");
}

} // namespace aion::gameserver::model::templates::challenge
