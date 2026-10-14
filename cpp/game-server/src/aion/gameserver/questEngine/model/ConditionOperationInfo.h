#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/questEngine/model/ConditionOperation.h"

namespace aion::gameserver::questEngine::model {

/**
 * Companion of the generated enum ConditionOperation (docs/design/static-data.md §2.5, pattern ConjunctionTypeInfo.h): Java's methods as free
 * functions: `value(type)` for Java `type.value()`, and `fromValueConditionOperation(v)` for the static `ConditionOperation.fromValue(v)`
 * (the name carries the type because both enums of this namespace have a static fromValue(String)).
 *
 * @author Mr. Poke
 */

/** Java: ConditionOperation.value() - name() */
constexpr std::string_view value(ConditionOperation type) noexcept {
	return xml::enumName(type);
}

/** Java: static ConditionOperation.fromValue(v) - valueOf(v). @throws IllegalArgumentException if there is no constant with that name */
inline ConditionOperation fromValueConditionOperation(std::string_view v) {
	return gameserver::model::templates::detail::enumValueOf<ConditionOperation>(v, "com.aionemu.gameserver.questEngine.model.ConditionOperation");
}

} // namespace aion::gameserver::questEngine::model
