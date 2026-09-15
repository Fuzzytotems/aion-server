#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/housing/PlaceLocation.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Companion of the generated enum PlaceLocation (docs/design/static-data.md §2.5): Java's methods as free functions (`value(x)` for Java
 * `x.value()`). The static `PlaceLocation.fromValue(value)` is `housing::fromValue<PlaceLocation>(value)`, since other enums of the
 * package declare a fromValue too.
 *
 * @author Rolandas
 */

/** Java value(): name() */
constexpr std::string_view value(PlaceLocation type) noexcept {
	return xml::enumName(type);
}

/** The static fromValue(String) of the housing enums (specialized per enum). @throws IllegalArgumentException for an unknown name */
template <class E>
E fromValue(std::string_view value);

/** Java static PlaceLocation.fromValue(value): valueOf(value) */
template <>
inline PlaceLocation fromValue<PlaceLocation>(std::string_view value) {
	return templates::detail::enumValueOf<PlaceLocation>(value, "com.aionemu.gameserver.model.templates.housing.PlaceLocation");
}

} // namespace aion::gameserver::model::templates::housing
