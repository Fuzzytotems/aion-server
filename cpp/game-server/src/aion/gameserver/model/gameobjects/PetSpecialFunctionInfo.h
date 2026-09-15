#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aion/gameserver/model/gameobjects/PetSpecialFunction.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Companion of the generated enum PetSpecialFunction (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(value)` for Java `value.getId()`).
 *
 * @author Neon
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 3> PETSPECIALFUNCTION_IDS{
	2, // DOPING
	3, // AUTOLOOT
	4, // AUTOSELL
};
static_assert(static_cast<size_t>(PetSpecialFunction::AUTOSELL) + 1 == PETSPECIALFUNCTION_IDS.size(), "one entry per PetSpecialFunction constant");
} // namespace detail

constexpr int32_t getId(PetSpecialFunction value) noexcept {
	return detail::PETSPECIALFUNCTION_IDS[static_cast<size_t>(value)];
}

/** @return the special function with the id, null (std::nullopt) if there is none */
constexpr std::optional<PetSpecialFunction> getById(int32_t id) noexcept {
	for (size_t i = 0; i < detail::PETSPECIALFUNCTION_IDS.size(); ++i) {
		if (detail::PETSPECIALFUNCTION_IDS[i] == id)
			return static_cast<PetSpecialFunction>(i);
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::gameobjects
