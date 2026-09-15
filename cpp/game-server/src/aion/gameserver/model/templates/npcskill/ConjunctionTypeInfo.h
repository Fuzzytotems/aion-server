#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/npcskill/ConjunctionType.h"

namespace aion::gameserver::model::templates::npcskill {

/**
 * Companion of the generated enum ConjunctionType (docs/design/static-data.md §2.5, pattern FoodTypeInfo.h): Java's methods as free functions
 * (`value(type)` for Java `type.value()`, `npcskill::fromValue(v)` for the static `ConjunctionType.fromValue(v)`).
 *
 * @author nrg
 */

/** Java value(): name() */
constexpr std::string_view value(ConjunctionType type) noexcept {
	return xml::enumName(type);
}

/** Java static fromValue(v): valueOf(v). @throws IllegalArgumentException if there is no constant with that name */
inline ConjunctionType fromValue(std::string_view v) {
	return templates::detail::enumValueOf<ConjunctionType>(v, "com.aionemu.gameserver.model.templates.npcskill.ConjunctionType");
}

} // namespace aion::gameserver::model::templates::npcskill
