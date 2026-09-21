#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * A stat whose additions reduce it (CreatureGameStats.getReverseStat), never below a base of 0.
 * <p>
 * C++: a K5 confined value class like Stat2.
 *
 * @author ATracer
 */
class ReverseStat : public Stat2 {
public:
	ReverseStat(container::StatEnum stat, float base, gameobjects::Creature& owner);

	void addToBase(float base) override;

	void addToBonus(float bonus) override;

	float calculatePercent(int32_t delta) override;
};

} // namespace aion::gameserver::model::stats::calc
