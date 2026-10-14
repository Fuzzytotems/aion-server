#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/questEngine/model/ConditionUnionType.h"

namespace aion::gameserver::questEngine::model {

/**
 * Companion of the generated enum ConditionUnionType (docs/design/static-data.md §2.5, pattern ConjunctionTypeInfo.h): Java's methods as free
 * functions: `value(type)` for Java `type.value()`, and `fromValueConditionUnionType(v)` for the static `ConditionUnionType.fromValue(v)`
 * (the name carries the type because both enums of this namespace have a static fromValue(String)).
 *
 * @author Mr. Poke
 */

/** Java: ConditionUnionType.value() - name() */
constexpr std::string_view value(ConditionUnionType type) noexcept {
	return xml::enumName(type);
}

/** Java: static ConditionUnionType.fromValue(v) - valueOf(v). @throws IllegalArgumentException if there is no constant with that name */
inline ConditionUnionType fromValueConditionUnionType(std::string_view v) {
	return gameserver::model::templates::detail::enumValueOf<ConditionUnionType>(v, "com.aionemu.gameserver.questEngine.model.ConditionUnionType");
}

} // namespace aion::gameserver::questEngine::model
