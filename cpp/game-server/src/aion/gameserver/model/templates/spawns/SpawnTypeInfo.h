#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/templates/spawns/SpawnType.h"

namespace aion::gameserver::model::templates::spawns {

/**
 * Companion of the generated enum SpawnType (docs/design/static-data.md §2.5): Java's methods as free functions (`value(type)` for Java
 * `type.value()`, `spawns::fromValue(v)` for the static `SpawnType.fromValue(v)`).
 *
 * @author Rolandas
 */

/** Java value(): name() */
constexpr std::string_view value(SpawnType type) noexcept {
	return xml::enumName(type);
}

/** Java static fromValue(v): valueOf(v). @throws IllegalArgumentException if there is no constant with that name */
inline SpawnType fromValue(std::string_view v) {
	std::optional<SpawnType> type = xml::enumFromName<SpawnType>(v);
	if (!type)
		throw commons::utils::IllegalArgumentException("No enum constant com.aionemu.gameserver.model.templates.spawns.SpawnType." + std::string(v));
	return *type;
}

} // namespace aion::gameserver::model::templates::spawns
