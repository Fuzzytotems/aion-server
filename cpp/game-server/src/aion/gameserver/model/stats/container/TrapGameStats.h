#pragma once

#include <memory>
#include <unordered_set>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * Java com.aionemu.gameserver.model.stats.container.TrapGameStats: the stats of a trap - the master's item bonus to
 * magic boost and magical accuracy (70 %), and the attack range and magical accuracy of each trap by its name.
 *
 * @author ATracer
 */
class TrapGameStats : public NpcGameStats {
public:
	explicit TrapGameStats(gameobjects::Npc& owner);
	~TrapGameStats() override;

	using CreatureGameStats::getStat;
	std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	std::unique_ptr<calc::Stat2> getAttackRange() override;

	std::unique_ptr<calc::Stat2> getMAccuracy() override;
};

} // namespace aion::gameserver::model::stats::container
