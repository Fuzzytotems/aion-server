#include "aion/gameserver/model/stats/container/SummonGameStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"

namespace aion::gameserver::model::stats::container {

SummonGameStats::SummonGameStats(gameobjects::Summon& ownerValue) : CreatureGameStats(ownerValue) {
}

SummonGameStats::~SummonGameStats() = default;

void SummonGameStats::onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

void SummonGameStats::updateStatsAndSpeedVisually() {
	AION_UNPORTED();
}

void SummonGameStats::updateStatsVisually() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

calc::Stat2& SummonGameStats::getStatWithBonusRate(StatEnum statEnum, calc::Stat2& stat, float bonusRate) {
	AION_UNPORTED();
}

const templates::stats::StatsTemplate* SummonGameStats::getStatsTemplate() {
	AION_UNPORTED();
}

int32_t SummonGameStats::getBaseAttackSpeed() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getMovementSpeed() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getAttackRange() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getHpRegenRate() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getMpRegenRate() {
	AION_UNPORTED();
}

void SummonGameStats::updateStatInfo() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::container
