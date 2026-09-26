#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/housing/HousingCategory.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Companion of the generated enum HousingCategory (docs/design/static-data.md §2.5): Java's methods as free functions (`value(x)` for Java
 * `x.value()`). The static `HousingCategory.fromValue(value)` is `housing::fromValue<HousingCategory>(value)`, since other enums of the
 * package declare a fromValue too.
 *
 * @author Rolandas
 */

/** Java value(): name() */
constexpr std::string_view value(HousingCategory type) noexcept {
	return xml::enumName(type);
}

/** The static fromValue(String) of the housing enums (specialized per enum). @throws IllegalArgumentException for an unknown name */
template <class E>
E fromValue(std::string_view value);

/** Java static HousingCategory.fromValue(value): valueOf(value) */
template <>
inline HousingCategory fromValue<HousingCategory>(std::string_view value) {
	return templates::detail::enumValueOf<HousingCategory>(value, "com.aionemu.gameserver.model.templates.housing.HousingCategory");
}

} // namespace aion::gameserver::model::templates::housing
