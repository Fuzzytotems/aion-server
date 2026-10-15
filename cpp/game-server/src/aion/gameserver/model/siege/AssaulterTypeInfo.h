#pragma once

#include <vector>

#include "aion/gameserver/model/siege/AssaulterType.h"

namespace aion::gameserver::model::siege {

/**
 * Companion of the generated enum AssaulterType (docs/design/static-data.md §2.5, AssaulterType.java): Java's constructor data as free
 * functions found by ADL (`getSpawnStake(type)` for Java `type.getSpawnStake()`). Pure data, so it is ported as a whole.
 */

/** Java: AssaulterType.getSpawnStake() - the constructor's spawnStake */
constexpr float getSpawnStake(AssaulterType type) noexcept {
	switch (type) {
		case AssaulterType::TELEPORT:
			return 0.0f;
		case AssaulterType::COMMANDER:
			return 0.0f;
		case AssaulterType::FIGHTER:
			return 0.3f;
		case AssaulterType::ASSASSIN:
			return 0.1f;
		case AssaulterType::RANGER:
			return 0.2f;
		case AssaulterType::WITCH:
			return 0.15f;
		case AssaulterType::PRIEST:
			return 0.1f;
		case AssaulterType::GUNNER:
			return 0.15f;
	}
	return 0.0f;
}

/** Java: AssaulterType.getSpawnCosts() - the constructor's spawnCosts */
inline std::vector<float> getSpawnCosts(AssaulterType type) {
	switch (type) {
		case AssaulterType::TELEPORT:
			return {};
		case AssaulterType::COMMANDER:
			return {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
		case AssaulterType::FIGHTER:
		case AssaulterType::ASSASSIN:
			return {0.2f, 0.4f, 0.8f, 1.0f};
		case AssaulterType::RANGER:
		case AssaulterType::GUNNER:
			return {0.4f, 0.8f, 1.6f, 2.0f};
		case AssaulterType::WITCH:
			return {0.5f, 1.0f, 2.0f, 2.5f};
		case AssaulterType::PRIEST:
			return {0.6f, 1.2f, 2.4f, 3.0f};
	}
	return {};
}

} // namespace aion::gameserver::model::siege
