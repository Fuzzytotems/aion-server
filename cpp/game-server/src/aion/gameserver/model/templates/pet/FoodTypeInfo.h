#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/templates/pet/FoodType.h"

namespace aion::gameserver::model::templates::pet {

/**
 * Companion of the generated enum FoodType (docs/design/static-data.md §2.5): Java's methods as free functions (`value(type)` for Java
 * `type.value()`, `pet::fromValue(name)` for the static `FoodType.fromValue(name)`).
 *
 * @author Rolandas
 */

/** Java value(): name() */
constexpr std::string_view value(FoodType type) noexcept {
	return xml::enumName(type);
}

/** Java static fromValue(value): valueOf(value). @throws IllegalArgumentException if there is no constant with that name */
inline FoodType fromValue(std::string_view value) {
	std::optional<FoodType> type = xml::enumFromName<FoodType>(value);
	if (!type)
		throw commons::utils::IllegalArgumentException("No enum constant com.aionemu.gameserver.model.templates.pet.FoodType." + std::string(value));
	return *type;
}

} // namespace aion::gameserver::model::templates::pet
