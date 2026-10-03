#pragma once

#include <memory>
#include <unordered_set>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * Java com.aionemu.gameserver.model.stats.container.HomingGameStats: the stats of a homing summoned object - the master's item bonus to magical attack (20 %) and the magical attack of each homing by name and skill level.
 *
 * @author Cheatkiller
 */
class HomingGameStats : public SummonedObjectGameStats {
public:
	explicit HomingGameStats(gameobjects::Npc& owner);
	~HomingGameStats() override;

	using CreatureGameStats::getStat;
	std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	using CreatureGameStats::getMainHandMAttack;
	std::unique_ptr<calc::Stat2> getMainHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;
};

} // namespace aion::gameserver::model::stats::container
