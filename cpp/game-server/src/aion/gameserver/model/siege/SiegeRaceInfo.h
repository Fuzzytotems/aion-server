#pragma once

#include <cstdint>

#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/siege/SiegeRace.h"

namespace aion::gameserver::model::siege {

/**
 * Companion of the generated enum SiegeRace (docs/design/static-data.md §2.5, SiegeRace.java): Java's constructor data and static methods as
 * free functions found by ADL (`getRaceId(siegeRace)` for Java `siegeRace.getRaceId()`). Pure data, so it is ported as a whole. Java's
 * SiegeRace implements L10n; the default method L10n.getL10n() is `ChatUtil::l10n(getL10nId(race))` at the call sites.
 *
 * The Java constructor data is `ELYOS(Race.ELYOS)`, `ASMODIANS(Race.ASMODIANS)` and `BALAUR(2, 900242)`, so the race id equals the ordinal.
 */

/** Java: SiegeRace.getRaceId() - ELYOS(Race.ELYOS: 0), ASMODIANS(Race.ASMODIANS: 1), BALAUR(2) */
constexpr int32_t getRaceId(SiegeRace race) noexcept {
	return static_cast<int32_t>(race);
}

/** Java: SiegeRace.getByRace(Race) */
constexpr SiegeRace getByRace(Race race) noexcept {
	switch (race) {
		case Race::ASMODIANS:
			return SiegeRace::ASMODIANS;
		case Race::ELYOS:
			return SiegeRace::ELYOS;
		default:
			return SiegeRace::BALAUR;
	}
}

/** Java: SiegeRace.getL10nId() (L10n) - the Race l10n ids of ELYOS and ASMODIANS, 900242 for BALAUR */
constexpr int32_t getL10nId(SiegeRace race) noexcept {
	switch (race) {
		case SiegeRace::ELYOS:
			return model::getL10nId(Race::ELYOS);
		case SiegeRace::ASMODIANS:
			return model::getL10nId(Race::ASMODIANS);
		default:
			return 900242;
	}
}

} // namespace aion::gameserver::model::siege
