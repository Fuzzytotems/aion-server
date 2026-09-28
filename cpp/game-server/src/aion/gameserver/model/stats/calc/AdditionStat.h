#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"

namespace aion::gameserver::model::stats::calc {

/**
 * A stat whose base and bonus additions add up (the default kind CreatureGameStats.getStat creates).
 * <p>
 * C++: a K5 confined value class like Stat2 (created per calculation and returned as `std::unique_ptr<Stat2>`, never stored in shared state).
 *
 * @author ATracer
 */
class AdditionStat : public Stat2 {
public:
	AdditionStat(container::StatEnum stat, float base, gameobjects::Creature& owner);

	void addToBase(float base) override final;

	void addToBonus(float bonus) override final;

	float calculatePercent(int32_t delta) override;
};

} // namespace aion::gameserver::model::stats::calc
