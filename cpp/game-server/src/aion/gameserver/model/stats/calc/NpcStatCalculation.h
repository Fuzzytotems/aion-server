#pragma once

#include <cstdint>

#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/npc/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * Base values of npc stats that the npc templates leave at 0 (NpcData.init).
 * <p>
 * C++: a static-only class (K5, thread-safe). Ported by P4-09 for the M4 load path (NpcData.init), the owning chunk P5-01 has no lane in
 * wave 3b-1. The float arithmetic follows Java's IEEE single precision (no contraction, Math.pow of small integers is exact), and Math.round is
 * utils::JavaMath::round.
 *
 * @author Estrayl, Neon
 */
class NpcStatCalculation final {
public:
	NpcStatCalculation() = delete;

	/** @throws IllegalArgumentException for a stat whose calculation is not implemented */
	static int32_t calculateStat(container::StatEnum stat, templates::npc::NpcRating rating, templates::npc::NpcRank rank, int8_t level);

private:
	static float getBaseValue(container::StatEnum stat, int8_t level);

	static float getRatingModifier(container::StatEnum stat, templates::npc::NpcRating rating);

	static float getRankModifier(container::StatEnum stat, templates::npc::NpcRank rank);
};

} // namespace aion::gameserver::model::stats::calc
