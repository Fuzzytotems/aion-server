#include "aion/gameserver/model/stats/container/NpcGameStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"

namespace aion::gameserver::model::stats::container {

NpcGameStats::NpcGameStats(gameobjects::Npc& ownerValue) : CreatureGameStats(ownerValue) {
}

NpcGameStats::~NpcGameStats() = default;

void NpcGameStats::onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

const templates::stats::StatsTemplate* NpcGameStats::getStatsTemplate() {
	AION_UNPORTED();
}

calc::Stat2& NpcGameStats::applyStatFunctions(StatEnum statEnum, calc::Stat2& stat,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t NpcGameStats::getBaseAttackSpeed() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> NpcGameStats::getMovementSpeed() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> NpcGameStats::getAttackRange() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> NpcGameStats::getHpRegenRate() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> NpcGameStats::getMpRegenRate() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getCastSpeed() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getLastAttackTimeDelta() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getLastAttackedTimeDelta() {
	AION_UNPORTED();
}

void NpcGameStats::renewLastAttackTime() {
	AION_UNPORTED();
}

void NpcGameStats::renewLastAttackedTime() {
	AION_UNPORTED();
}

bool NpcGameStats::isNextAttackScheduled() {
	AION_UNPORTED();
}

void NpcGameStats::setFightStartingTime() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getNextAttackInterval() {
	AION_UNPORTED();
}

void NpcGameStats::renewLastSkillTime() {
	AION_UNPORTED();
}

void NpcGameStats::renewLastChangeTargetTime() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getLastSkillTimeDelta() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getLastChangeTargetTimeDelta() {
	AION_UNPORTED();
}

bool NpcGameStats::canUseNextSkill() {
	AION_UNPORTED();
}

void NpcGameStats::setNextSkillDelay(int32_t value) {
	AION_UNPORTED();
}

void NpcGameStats::setLastSkill(runtime::Ptr<skill::NpcSkillEntry> value) {
	lastSkill.set(value);
}

void NpcGameStats::resetFightStats() {
	AION_UNPORTED();
}

int32_t NpcGameStats::getInitialSkillDelay() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::container
