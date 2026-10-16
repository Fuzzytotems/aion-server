#include "aion/gameserver/model/stats/container/ServantGameStats.h"

#include <cstdint>
#include <memory>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::stats::container {

ServantGameStats::ServantGameStats(gameobjects::Npc& ownerValue) : SummonedObjectGameStats(ownerValue) {
}

ServantGameStats::~ServantGameStats() = default;

std::unique_ptr<calc::Stat2> ServantGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	return SummonedObjectGameStats::getStat(statEnum, statEnum == StatEnum::HEAL_BOOST ? static_cast<float>(fixedHealBoost.get()) : base,
		calculationTypes);
}

std::unique_ptr<calc::Stat2> ServantGameStats::getMBoost() {
	return CreatureGameStats::getStat(StatEnum::BOOST_MAGICAL_SKILL, static_cast<float>(fixedMBoost.get()));
}

std::unique_ptr<calc::Stat2> ServantGameStats::getMAccuracy() {
	return CreatureGameStats::getStat(StatEnum::MAGICAL_ACCURACY, static_cast<float>(fixedMagicalAccuracy.get()));
}

void ServantGameStats::setUpStats() {
	setFixedMBoost();
	setFixedHealBoost();
	setFixedMagicalAccuracy();
}

void ServantGameStats::setFixedMBoost() {
	fixedMBoost = static_cast<gameobjects::Npc&>(owner).getMaster()->getGameStats()->getMBoost()->getBonus();
}

// Java super.getStat(StatEnum, int): CreatureGameStats' two-argument form, which dispatches to this class's getStat
void ServantGameStats::setFixedHealBoost() {
	std::unique_ptr<calc::Stat2> healBoostStat = CreatureGameStats::getStat(StatEnum::HEAL_BOOST, 0.0f);
	healBoostStat->setBonusRate(0.5f);
	int32_t healBoost = static_cast<gameobjects::Npc&>(owner).getMaster()->getGameStats()->getItemStatBoost(StatEnum::HEAL_BOOST, *healBoostStat).getCurrent();
	if (healBoost > 500) {
		healBoost = 500;
	}
	fixedHealBoost = healBoost;
}

void ServantGameStats::setFixedMagicalAccuracy() {
	const templates::stats::StatsTemplate* statsTemplate = getStatsTemplate();
	if (statsTemplate == nullptr) // Java: getStatsTemplate().getMacc() - NpcTemplate.statsTemplate is nullable
		throw runtime::NullPointerException("the servant has no stats template");
	std::unique_ptr<calc::Stat2> magicalAccuracyStat = CreatureGameStats::getStat(StatEnum::MAGICAL_ACCURACY, static_cast<float>(statsTemplate->getMacc()));
	magicalAccuracyStat->setBaseRate(1.2f);
	fixedMagicalAccuracy = static_cast<gameobjects::Npc&>(owner)
							   .getMaster()
							   ->getGameStats()
							   ->getItemStatBoost(StatEnum::MAGICAL_ACCURACY, *magicalAccuracyStat)
							   .getCurrent();
}

} // namespace aion::gameserver::model::stats::container
