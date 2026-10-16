#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/challenge/RewardType.h"
#include "aion/gameserver/model/templates/detail/EnumValueOf.h"

namespace aion::gameserver::model::templates::challenge {

/**
 * Companion of the generated enum RewardType (docs/design/static-data.md §2.5): Java's methods as free functions (`value(type)` for Java
 * `type.value()`). The static `RewardType.fromValue(name)` is `challenge::fromValue<RewardType>(name)`, since ChallengeType declares a fromValue
 * too.
 */

/** Java value(): name() */
constexpr std::string_view value(RewardType type) noexcept {
	return xml::enumName(type);
}

/** The static fromValue(String) of the challenge enums (specialized per enum). @throws IllegalArgumentException for an unknown name */
template <class E>
E fromValue(std::string_view paramString);

/** Java static RewardType.fromValue(paramString): valueOf(paramString) */
template <>
inline RewardType fromValue<RewardType>(std::string_view paramString) {
	return templates::detail::enumValueOf<RewardType>(paramString, "com.aionemu.gameserver.model.templates.challenge.RewardType");
}

} // namespace aion::gameserver::model::templates::challenge
