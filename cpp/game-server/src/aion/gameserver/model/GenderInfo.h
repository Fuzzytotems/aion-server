#pragma once

#include <cstdint>

#include "aion/gameserver/model/Gender.h"

namespace aion::gameserver::model {

/** Companion of the generated enum Gender (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

/** Java: Gender.getGenderId() - MALE 0, FEMALE 1 (equal to the ordinal) */
constexpr int32_t getGenderId(Gender gender) noexcept {
	return gender == Gender::MALE ? 0 : 1;
}

} // namespace aion::gameserver::model
