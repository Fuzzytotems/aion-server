#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Companion of the generated enum LetterType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(value)` for Java `value.getId()`).
 *
 * @author ginho1
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 3> LETTERTYPE_IDS{
	0, // NORMAL
	1, // EXPRESS
	2, // BLACKCLOUD
};
static_assert(static_cast<size_t>(LetterType::BLACKCLOUD) + 1 == LETTERTYPE_IDS.size(), "one entry per LetterType constant");
} // namespace detail

constexpr int32_t getId(LetterType value) noexcept {
	return detail::LETTERTYPE_IDS[static_cast<size_t>(value)];
}

/** @throws IllegalArgumentException for an unknown id (Java's message, which says "revive type") */
inline LetterType getLetterTypeById(int32_t id) {
	for (size_t i = 0; i < detail::LETTERTYPE_IDS.size(); ++i) {
		if (detail::LETTERTYPE_IDS[i] == id)
			return static_cast<LetterType>(i);
	}
	throw runtime::IllegalArgumentException("Unsupported revive type: " + std::to_string(id));
}

} // namespace aion::gameserver::model::gameobjects
