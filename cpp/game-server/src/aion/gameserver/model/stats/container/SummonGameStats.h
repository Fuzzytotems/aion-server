#pragma once

#include <cstdint>
#include <memory>
#include <unordered_set>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The game stats part of Summon (Summon::postConstruct). Java makes the overridden
 * 3-argument getStat public; the 2-argument base overload stays visible through a using-declaration.
 *
 * @author ATracer
 */
class SummonGameStats : public CreatureGameStats {
public:
	explicit SummonGameStats(gameobjects::Summon& owner);
	~SummonGameStats() override;

protected:
	void onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) override;

public:
	void updateStatsAndSpeedVisually();

	void updateStatsVisually();

	using CreatureGameStats::getStat;

	std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

private:
	/** @return `stat` itself (Java returns the argument) */
	calc::Stat2& getStatWithBonusRate(StatEnum statEnum, calc::Stat2& stat, float bonusRate);

public:
	const templates::stats::StatsTemplate* getStatsTemplate() override;

	int32_t getBaseAttackSpeed() override;

	std::unique_ptr<calc::Stat2> getMovementSpeed() override;

	std::unique_ptr<calc::Stat2> getAttackRange() override;

	std::unique_ptr<calc::Stat2> getHpRegenRate() override;

	std::unique_ptr<calc::Stat2> getMpRegenRate() override;

	void updateStatInfo() override;
};

} // namespace aion::gameserver::model::stats::container
