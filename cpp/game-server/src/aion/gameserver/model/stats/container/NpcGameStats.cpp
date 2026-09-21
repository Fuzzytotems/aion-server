#include "aion/gameserver/model/stats/container/NpcGameStats.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::model::stats::container {

using gameobjects::state::CreatureState;

namespace {

/** Java: Math.round((System.currentTimeMillis() - time) / 1000f) */
int32_t secondsSince(int64_t time) {
	return utils::JavaMath::round((commons::utils::currentTimeMillis() - time) / 1000.0f);
}

/** Java: getObjectTemplate().getX() - a NullPointerException where the npc has no template (like CreatureGameStats::nonNull) */
const templates::npc::NpcTemplate& nonNull(const templates::npc::NpcTemplate* npcTemplate) {
	if (npcTemplate == nullptr)
		throw runtime::NullPointerException("the npc has no object template");
	return *npcTemplate;
}

/** Java: getStatsTemplate().getX() - NpcTemplate.statsTemplate is a nullable field (an npc_template without a <stats> element) */
const templates::stats::StatsTemplate& nonNull(const templates::stats::StatsTemplate* statsTemplate) {
	if (statsTemplate == nullptr)
		throw runtime::NullPointerException("the npc has no stats template");
	return *statsTemplate;
}

} // namespace

NpcGameStats::NpcGameStats(gameobjects::Npc& ownerValue) : CreatureGameStats(ownerValue) {
}

NpcGameStats::~NpcGameStats() = default;

void NpcGameStats::onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) {
	CreatureGameStats::onStatsChange(effect);
	checkSpeedStats();
}

const templates::stats::StatsTemplate* NpcGameStats::getStatsTemplate() {
	return nonNull(static_cast<gameobjects::Npc&>(owner).getObjectTemplate()).getStatsTemplate();
}

calc::Stat2& NpcGameStats::applyStatFunctions(StatEnum statEnum, calc::Stat2& stat,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	calc::Stat2& s = CreatureGameStats::applyStatFunctions(statEnum, stat, calculationTypes);
	owner.getAi().modifyOwnerStat(s);
	return s;
}

int32_t NpcGameStats::getBaseAttackSpeed() {
	return nonNull(static_cast<gameobjects::Npc&>(owner).getObjectTemplate()).getAttackSpeed();
}

std::unique_ptr<calc::Stat2> NpcGameStats::getMovementSpeed() {
	gameobjects::Npc& npc = static_cast<gameobjects::Npc&>(owner);
	const templates::stats::StatsTemplate& statsTemplate = nonNull(getStatsTemplate());
	std::unique_ptr<calc::Stat2> newSpeedStat;
	if (npc.isInState(CreatureState::WEAPON_EQUIPPED)) {
		float speed;
		if (npc.getWalkerGroup())
			speed = statsTemplate.getGroupRunSpeedFight();
		else
			speed = statsTemplate.getRunSpeedFight();
		newSpeedStat = getStat(StatEnum::SPEED, static_cast<float>(utils::JavaMath::round(speed * 1000)));
	} else if (npc.isInState(CreatureState::WALK_MODE)) {
		float speed;
		if (npc.getWalkerGroup() && npc.getAi().getSubState() == ai::AISubState::WALK_PATH)
			speed = statsTemplate.getGroupWalkSpeed();
		else
			speed = statsTemplate.getWalkSpeed();
		newSpeedStat = getStat(StatEnum::SPEED, static_cast<float>(utils::JavaMath::round(speed * 1000)));
	} else {
		float multiplier = npc.isFlying() ? 1.3f : 1.0f;
		newSpeedStat = getStat(StatEnum::SPEED, static_cast<float>(utils::JavaMath::round(statsTemplate.getRunSpeed() * multiplier * 1000)));
	}
	return newSpeedStat;
}

std::unique_ptr<calc::Stat2> NpcGameStats::getAttackRange() {
	// Java: getAttackRange() * 1000 is an int product
	int32_t attackRange = nonNull(static_cast<gameobjects::Npc&>(owner).getObjectTemplate()).getAttackRange();
	return getStat(StatEnum::ATTACK_RANGE, static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(attackRange) * 1000u)));
}

std::unique_ptr<calc::Stat2> NpcGameStats::getHpRegenRate() {
	int32_t divider = 2;
	if (static_cast<gameobjects::Npc&>(owner).getAbyssNpcType() != templates::npc::AbyssNpcType::NONE)
		divider = 4; // Abyss type related NPCs restore their health by 25%
	return getStat(StatEnum::REGEN_HP, static_cast<float>(nonNull(getStatsTemplate()).getMaxHp() / divider));
}

std::unique_ptr<calc::Stat2> NpcGameStats::getMpRegenRate() {
	throw runtime::IllegalStateException("No mp regen for NPC");
}

int32_t NpcGameStats::getCastSpeed() {
	return nonNull(static_cast<gameobjects::Npc&>(owner).getObjectTemplate()).getCastSpeed();
}

int32_t NpcGameStats::getLastAttackTimeDelta() {
	return secondsSince(lastAttackTime.get());
}

int32_t NpcGameStats::getLastAttackedTimeDelta() {
	return secondsSince(lastAttackedTime.get());
}

void NpcGameStats::renewLastAttackTime() {
	this->lastAttackTime = commons::utils::currentTimeMillis();
}

void NpcGameStats::renewLastAttackedTime() {
	this->lastAttackedTime = commons::utils::currentTimeMillis();
}

bool NpcGameStats::isNextAttackScheduled() {
	return nextAttackTime.get() - commons::utils::currentTimeMillis() > 50;
}

void NpcGameStats::setFightStartingTime() {
	this->fightStartingTime = commons::utils::currentTimeMillis();
}

int32_t NpcGameStats::getNextAttackInterval() {
	AION_UNPORTED();
}

void NpcGameStats::renewLastSkillTime() {
	this->lastSkillTime = commons::utils::currentTimeMillis();
}

void NpcGameStats::renewLastChangeTargetTime() {
	this->lastChangeTarget = commons::utils::currentTimeMillis();
}

int32_t NpcGameStats::getLastSkillTimeDelta() {
	return secondsSince(lastSkillTime.get());
}

int32_t NpcGameStats::getLastChangeTargetTimeDelta() {
	return secondsSince(lastChangeTarget.get());
}

bool NpcGameStats::canUseNextSkill() {
	return nextSkillDelay.get() == 0 || commons::utils::currentTimeMillis() >= lastSkillTime.get() + nextSkillDelay.get();
}

void NpcGameStats::setNextSkillDelay(int32_t value) {
	if (value == -1) // xml skills without specific times in templates
		this->nextSkillDelay = commons::utils::Rnd::get(3000, 9000);
	else
		this->nextSkillDelay = value;
}

void NpcGameStats::setLastSkill(runtime::Ptr<skill::NpcSkillEntry> value) {
	lastSkill.set(value);
}

void NpcGameStats::resetFightStats() {
	lastAttackTime = 0;
	lastAttackedTime = 0;
	lastChangeTarget = 0;
	fightStartingTime = 0;
	nextAttackTime = 0;
	lastSkillTime = 0;
	nextSkillDelay = 0;
}

int32_t NpcGameStats::getInitialSkillDelay() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::container
