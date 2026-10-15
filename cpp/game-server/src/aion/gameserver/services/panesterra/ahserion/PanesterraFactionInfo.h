#pragma once

#include <cstdint>

#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::services::panesterra::ahserion {

/**
 * Companion of the generated enum PanesterraFaction (docs/design/static-data.md §2.5, PanesterraFaction.java): its static methods as free
 * functions. M5i I-10.
 */

/** Java: PanesterraFaction.getByFortressId(int) */
constexpr PanesterraFaction getByFortressId(int32_t fortressId) noexcept {
	switch (fortressId) {
		case 10111:
			return PanesterraFaction::BELUS;
		case 10211:
			return PanesterraFaction::ASPIDA;
		case 10311:
			return PanesterraFaction::ATANATOS;
		case 10411:
			return PanesterraFaction::DISILLON;
		default:
			return PanesterraFaction::PEACE;
	}
}

} // namespace aion::gameserver::services::panesterra::ahserion
