#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"

namespace aion::gameserver::model::templates::npcshout {

/**
 * Companion of the generated enum ShoutEventType (docs/design/static-data.md §2.5, pattern FoodTypeInfo.h): Java's methods as free functions
 * (`value(type)` for Java `type.value()`, `npcshout::fromValue(v)` for the static `ShoutEventType.fromValue(v)`).
 *
 * @author Rolandas
 */

/** Java value(): name() */
constexpr std::string_view value(ShoutEventType type) noexcept {
	return xml::enumName(type);
}

/** Java static fromValue(v): valueOf(v). @throws IllegalArgumentException if there is no constant with that name */
inline ShoutEventType fromValue(std::string_view v) {
	return templates::detail::enumValueOf<ShoutEventType>(v, "com.aionemu.gameserver.model.templates.npcshout.ShoutEventType");
}

} // namespace aion::gameserver::model::templates::npcshout
