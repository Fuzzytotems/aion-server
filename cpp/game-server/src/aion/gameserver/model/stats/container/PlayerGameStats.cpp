#include "aion/gameserver/model/stats/container/PlayerGameStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"

namespace aion::gameserver::model::stats::container {

PlayerGameStats::PlayerGameStats(gameobjects::player::Player& ownerValue) : CreatureGameStats(ownerValue) {
	// Java: updateStatsTemplate() (owner.getPlayerClass().createStatsTemplate(owner.getLevel()), static data)
	AION_UNPORTED();
}

PlayerGameStats::~PlayerGameStats() = default;

void PlayerGameStats::onStatsChange(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

void PlayerGameStats::updateStatsAndSpeedVisually() {
	AION_UNPORTED();
}

void PlayerGameStats::updateStatsVisually() {
	AION_UNPORTED();
}

bool PlayerGameStats::checkSpeedStats() {
	AION_UNPORTED();
}

void PlayerGameStats::updateStatsTemplate() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMaxDp() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getFlyTime() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getBaseAttackSpeed() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMovementSpeed() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getAttackRange() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getParry() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMainHandPAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getOffHandPAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getOffHandPAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMainHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getOffHandMAttack(std::initializer_list<utils::stats::CalculationType> calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getOffHandMAttack(const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMainHandPCritical() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getOffHandPCritical() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMainHandPAccuracy() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getOffHandPAccuracy() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMBoost() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMAccuracy() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMCritical() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getHpRegenRate() {
	AION_UNPORTED();
}

std::unique_ptr<calc::Stat2> PlayerGameStats::getMpRegenRate() {
	AION_UNPORTED();
}

void PlayerGameStats::updateStatInfo() {
	AION_UNPORTED();
}

void PlayerGameStats::updateSpeedInfo() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getHealthDependentAdditionalHp() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getWillDependentAdditionalMp() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getAgilityDependentAdditionalBaseBlock() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getAgilityDependentAdditionalBaseParry() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getAgilityDependentAdditionalBaseEvasion() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getAccuracyDependentAdditionalBasePhysicalAccuracy() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getAccuracyDependentAdditionalBasePhysicalCritical() {
	AION_UNPORTED();
}

int32_t PlayerGameStats::calculateBaseStatDependentAdditionalValue(calc::Stat2& baseStat, int32_t multiplier) {
	AION_UNPORTED();
}

int32_t PlayerGameStats::getPowerShardDamage(bool mainHand, bool removePowerShards) {
	AION_UNPORTED();
}

float PlayerGameStats::getOffHandDamageRatio() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::container
