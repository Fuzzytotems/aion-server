#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "aion/gameserver/world/zone/ZoneAttributes.h"

namespace aion::gameserver::world::zone {

/** Companion of the generated enum ZoneAttributes (static-data.md §2.5): Java's constructor data and static methods as free functions (ADL). */

/** Java: ZoneAttributes.getId(): the constructor argument, `1 << ordinal` for every constant (BIND 1 << 0 ... NO_RETURN_BATTLE 1 << 9) */
constexpr int32_t getId(ZoneAttributes attribute) noexcept {
	return int32_t{1} << static_cast<int32_t>(attribute);
}

/** Java: ZoneAttributes.fromList(flagValues): the ids of the listed attributes or-ed together (Java NullPointerException for a null list) */
inline int32_t fromList(const std::vector<ZoneAttributes>& flagValues) {
	int32_t result = 0;
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<ZoneAttributes>::names.size(); ++ordinal) {
		ZoneAttributes attribute = static_cast<ZoneAttributes>(ordinal);
		if (std::find(flagValues.begin(), flagValues.end(), attribute) != flagValues.end())
			result |= getId(attribute);
	}
	return result;
}

} // namespace aion::gameserver::world::zone
