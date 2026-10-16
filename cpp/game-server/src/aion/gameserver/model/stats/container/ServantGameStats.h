#pragma once

#include <cstdint>
#include <memory>
#include <unordered_set>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * Java com.aionemu.gameserver.model.stats.container.ServantGameStats: the stats of a servant, a summoned object whose magic boost, heal boost and magical accuracy are fixed from its master's at spawn (setUpStats, Servant.setUpStats).
 *
 * @author Yeats
 */
class ServantGameStats : public SummonedObjectGameStats {
private:
	runtime::Field<int32_t> fixedMBoost{0};
	runtime::Field<int32_t> fixedHealBoost{0};
	runtime::Field<int32_t> fixedMagicalAccuracy{0};

public:
	explicit ServantGameStats(gameobjects::Npc& owner);
	~ServantGameStats() override;

	using CreatureGameStats::getStat;
	std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	std::unique_ptr<calc::Stat2> getMBoost() override;

	std::unique_ptr<calc::Stat2> getMAccuracy() override;

	// TODO: there might be more stats which are set only at spawn
	void setUpStats();

private:
	void setFixedMBoost();

	void setFixedHealBoost();

	void setFixedMagicalAccuracy();
};

} // namespace aion::gameserver::model::stats::container
