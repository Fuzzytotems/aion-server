#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::model::base {

/**
 * Companion of the generated enum BaseOccupier (docs/design/static-data.md §2.5, BaseOccupier.java): Java's constructor data and static
 * methods as free functions found by ADL. Pure data, so it is ported as a whole.
 *
 * The Java constructor data is ELYOS() and ASMODIANS() without a faction, then BALAUR(PanesterraFaction.BALAUR) ... PEACE(PanesterraFaction.PEACE)
 * in PanesterraFaction's own order, so the faction of the occupier with ordinal n >= 2 is the faction with ordinal n - 2.
 */

/** Java: BaseOccupier.getPanesterraFaction() - null (std::nullopt) for ELYOS and ASMODIANS */
constexpr std::optional<services::panesterra::ahserion::PanesterraFaction> getPanesterraFaction(BaseOccupier occupier) noexcept {
	const auto ordinal = static_cast<uint8_t>(occupier);
	if (ordinal < 2)
		return std::nullopt;
	return static_cast<services::panesterra::ahserion::PanesterraFaction>(ordinal - 2);
}

/** Java: BaseOccupier.findBy(PanesterraFaction) - the first occupier with that faction (every faction has one) */
constexpr BaseOccupier findBy(services::panesterra::ahserion::PanesterraFaction panesterraFaction) noexcept {
	return static_cast<BaseOccupier>(static_cast<uint8_t>(panesterraFaction) + 2);
}

/** Java: BaseOccupier.findBy(Race) */
constexpr BaseOccupier findBy(Race race) noexcept {
	switch (race) {
		case Race::ELYOS:
			return BaseOccupier::ELYOS;
		case Race::ASMODIANS:
			return BaseOccupier::ASMODIANS;
		default:
			return BaseOccupier::BALAUR;
	}
}

static_assert(findBy(services::panesterra::ahserion::PanesterraFaction::BALAUR) == BaseOccupier::BALAUR);
static_assert(findBy(services::panesterra::ahserion::PanesterraFaction::PEACE) == BaseOccupier::PEACE);
static_assert(findBy(services::panesterra::ahserion::PanesterraFaction::ILLUMINATUS_TEMPLE) == BaseOccupier::ILLUMINATUS_TEMPLE);

} // namespace aion::gameserver::model::base
