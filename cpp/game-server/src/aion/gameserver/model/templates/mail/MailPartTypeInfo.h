#pragma once

#include <string_view>

#include "aion/gameserver/model/templates/detail/EnumValueOf.h"
#include "aion/gameserver/model/templates/mail/MailPartType.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Companion of the generated enum MailPartType (docs/design/static-data.md §2.5, pattern FoodTypeInfo.h): Java's methods as free functions
 * (`value(type)` for Java `type.value()`, `mail::fromValue(name)` for the static `MailPartType.fromValue(name)`).
 *
 * @author Rolandas
 */

/** Java value(): name() */
constexpr std::string_view value(MailPartType type) noexcept {
	return xml::enumName(type);
}

/** Java static fromValue(v): valueOf(v). @throws IllegalArgumentException if there is no constant with that name */
inline MailPartType fromValue(std::string_view v) {
	return templates::detail::enumValueOf<MailPartType>(v, "com.aionemu.gameserver.model.templates.mail.MailPartType");
}

} // namespace aion::gameserver::model::templates::mail
