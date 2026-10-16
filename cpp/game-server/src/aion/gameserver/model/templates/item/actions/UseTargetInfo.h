#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/item/actions/UseTarget.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Companion of the generated enum UseTarget (docs/design/static-data.md §2.5): Java's methods as free functions (`value(x)` for Java
 * `x.value()`). The static `UseTarget.fromValue(v)` is `actions::fromValue<UseTarget>(v)`, the pattern of the housing enums (PlaceAreaInfo.h).
 */

/** Java value(): name() */
constexpr std::string_view value(UseTarget type) noexcept {
	return xml::enumName(type);
}

/** The static fromValue(String) of the item action enums (specialized per enum). @throws IllegalArgumentException for an unknown name */
template <class E>
E fromValue(std::string_view v);

/** Java static UseTarget.fromValue(v): valueOf(v) */
template <>
inline UseTarget fromValue<UseTarget>(std::string_view v) {
	return templates::detail::enumValueOf<UseTarget>(v, "com.aionemu.gameserver.model.templates.item.actions.UseTarget");
}

} // namespace aion::gameserver::model::templates::item::actions
