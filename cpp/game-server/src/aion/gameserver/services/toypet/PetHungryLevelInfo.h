#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/toypet/PetHungryLevel.h"

namespace aion::gameserver::services::toypet {

/**
 * Companion of the generated enum PetHungryLevel (docs/design/static-data.md §2.5, pattern CreatureStateInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`getValue(level)` for Java `level.getValue()`, `getNextValue(level)`); `petHungryLevelFromId` is
 * the static PetHungryLevel.fromId.
 */

/** Java PetHungryLevel(int value): the byte of each constant, 0..3 in ordinal order (PetHungryLevel.java:8-11) */
constexpr int8_t getValue(PetHungryLevel level) noexcept {
	return static_cast<int8_t>(level);
}

/** Java PetHungryLevel.getNextValue (PetHungryLevel.java:24-38): HUNGRY -> CONTENT -> SEMIFULL -> FULL -> HUNGRY */
constexpr PetHungryLevel getNextValue(PetHungryLevel level) noexcept {
	switch (getValue(level)) {
		case 0:
			return PetHungryLevel::CONTENT;
		case 1:
			return PetHungryLevel::SEMIFULL;
		case 2:
			return PetHungryLevel::FULL;
		case 3:
			return PetHungryLevel::HUNGRY;
		default:
			return PetHungryLevel::HUNGRY;
	}
}

/**
 * Java PetHungryLevel.fromId(value): PetHungryLevel.values()[value] (PetHungryLevel.java:40-42)
 *
 * @throws ArrayIndexOutOfBoundsException outside 0..3
 */
inline PetHungryLevel petHungryLevelFromId(int32_t value) {
	constexpr int32_t count = 4; // HUNGRY, CONTENT, SEMIFULL, FULL
	if (value < 0 || value >= count)
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(value) + " out of bounds for length " + std::to_string(count));
	return static_cast<PetHungryLevel>(value);
}

} // namespace aion::gameserver::services::toypet
