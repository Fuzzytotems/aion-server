#include "aion/gameserver/model/stats/container/CreatureGameStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor binds the OwnedPart owner (Creature must be complete to convert to
// RefCounted&) and the destructor releases the Ref<IStatFunction> elements of `stats`. Creature.h is written by the objects group,
// IStatFunction.h by a later stage. Remove the guard once both exist (spine freeze).
#if __has_include("aion/gameserver/model/gameobjects/Creature.h") && __has_include("aion/gameserver/model/stats/calc/functions/IStatFunction.h")
#define AION_S0B_CREATURE_GAME_STATS_PARTS 1
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#else
#define AION_S0B_CREATURE_GAME_STATS_PARTS 0
#endif

namespace aion::gameserver::model::stats::container {

#if AION_S0B_CREATURE_GAME_STATS_PARTS
CreatureGameStats::CreatureGameStats(gameobjects::Creature& value) : runtime::OwnedPart(value), owner(value) {
}

CreatureGameStats::~CreatureGameStats() = default;
#endif

void CreatureGameStats::setAttackCounter(int32_t value) {
	AION_UNPORTED();
}

void CreatureGameStats::increaseAttackCounter() {
	AION_UNPORTED();
}

// lint: L7 unported body (AION_UNPORTED); the port restores Java's synchronized block
void CreatureGameStats::addEffectOnly(runtime::Ptr<calc::StatOwner> statOwner, const std::vector<runtime::Ptr<calc::functions::IStatFunction>>& functions) {
	AION_UNPORTED();
}

void CreatureGameStats::addEffect(runtime::Ptr<calc::StatOwner> statOwner, const std::vector<runtime::Ptr<calc::functions::IStatFunction>>& functions) {
	AION_UNPORTED();
}

// lint: L7 unported body (AION_UNPORTED); the port restores Java's synchronized block
void CreatureGameStats::endEffect(calc::StatOwner& statOwner) {
	AION_UNPORTED();
}

void CreatureGameStats::clearEffectFunctionsWithoutNotify() {
	AION_UNPORTED();
}

float CreatureGameStats::getPositiveStat(StatEnum statEnum, float base) {
	AION_UNPORTED();
}

int32_t CreatureGameStats::getPositiveReverseStat(StatEnum statEnum, int32_t base) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getStat(StatEnum statEnum, float base) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getReverseStat(StatEnum statEnum, float base) {
	AION_UNPORTED();
}

calc::Stat2& CreatureGameStats::applyStatFunctions(StatEnum statEnum, calc::Stat2& stat,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

calc::Stat2& CreatureGameStats::getItemStatBoost(StatEnum statEnum, calc::Stat2& stat) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getPower() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getHealth() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getAccuracy() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getAgility() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getKnowledge() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getWill() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMaxHp() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMaxMp() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getPDef() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMDef() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getEvasion() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getParry() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getBlock() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMResist() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getPCR() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMCR() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMainHandPAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMainHandPAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMainHandPCritical() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMainHandPAccuracy() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMainHandMAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMainHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMCritical() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMAccuracy() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMBoost() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getMBResist() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getAbnormalResistance() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getResistance(StatEnum statEnum) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> CreatureGameStats::getAttackSpeed() {
	AION_UNPORTED();
}

float CreatureGameStats::getAttackSpeedRate() {
	AION_UNPORTED();
}

int32_t CreatureGameStats::getElementalDefenseFor(SkillElement element) {
	AION_UNPORTED();
}

float CreatureGameStats::getMovementSpeedFloat() {
	AION_UNPORTED();
}

void CreatureGameStats::updateArmorMasteryStats(const std::vector<runtime::Ptr<gameobjects::Item>>& equipment) {
	AION_UNPORTED();
}

void CreatureGameStats::updateStatInfo() {
	AION_UNPORTED();
}

void CreatureGameStats::updateSpeedInfo() {
	AION_UNPORTED();
}

bool CreatureGameStats::checkSpeedStats() {
	AION_UNPORTED();
}

// lint: L7 unported body (AION_UNPORTED); the port restores Java's synchronized block
std::vector<runtime::Ptr<calc::functions::IStatFunction>> CreatureGameStats::getStatsSorted(StatEnum stat) {
	AION_UNPORTED();
}

void CreatureGameStats::onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

// lint: L7 unported body (AION_UNPORTED); the port restores Java's synchronized block
void CreatureGameStats::checkMaxHPChanged(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

// lint: L7 unported body (AION_UNPORTED); the port restores Java's synchronized block
void CreatureGameStats::checkMaxMPChanged(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

std::unordered_set<utils::stats::CalculationType> CreatureGameStats::toSet(std::span<const utils::stats::CalculationType> calculationTypes) {
	AION_UNPORTED();
}

std::unordered_set<utils::stats::CalculationType> CreatureGameStats::copyWith(const std::unordered_set<utils::stats::CalculationType>& types,
	utils::stats::CalculationType type) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::container
