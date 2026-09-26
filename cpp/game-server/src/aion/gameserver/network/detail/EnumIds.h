#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::network::detail {

/**
 * C++ only (P4-15): the constructor data of generated model enums that the network writers read, standing in for their companion headers
 * (docs/design/static-data.md §2.5; owned by P4-05, not in the tree yet). Pure data copied from the Java enums. TODO(P4-05): use getRaceId,
 * getClassId and getGenderId of RaceInfo.h, PlayerClassInfo.h and GenderInfo.h once they exist.
 */

/** Java: Race.getRaceId() (Race.java constructor arguments in ordinal order) */
constexpr int32_t raceIdOf(model::Race race) noexcept {
	static constexpr std::array<int8_t, 48> RACE_IDS{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
		28, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46};
	const auto index = static_cast<size_t>(race);
	return index < RACE_IDS.size() ? RACE_IDS[index] : static_cast<int32_t>(index);
}

/** Java: PlayerClass.getClassId() (the class id equals the ordinal, PlayerClass.java) */
constexpr int32_t classIdOf(model::PlayerClass playerClass) noexcept {
	return static_cast<int32_t>(playerClass);
}

/** Java: Gender.getGenderId() (MALE(0), FEMALE(1)) */
constexpr int32_t genderIdOf(model::Gender gender) noexcept {
	return static_cast<int32_t>(gender);
}

} // namespace aion::gameserver::network::detail
