#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::container {

CreatureLifeStats::CreatureLifeStats(gameobjects::Creature& value, int32_t currentHpValue, int32_t currentMpValue)
	: runtime::OwnedPart(value), currentHp(currentHpValue), currentMp(currentMpValue), owner(value) {
}

CreatureLifeStats::~CreatureLifeStats() = default;

int32_t CreatureLifeStats::getMaxHp() {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::getMaxMp() {
	AION_UNPORTED();
}

bool CreatureLifeStats::isDead() {
	AION_UNPORTED();
}

bool CreatureLifeStats::isAboutToDie() {
	AION_UNPORTED();
}

void CreatureLifeStats::unsetIsAboutToDie() {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::reduceHp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, gameobjects::Creature& attacker) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::reduceHp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, gameobjects::Creature& attacker, bool criticalHit) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::reduceMp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, int32_t skillId,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) {
	AION_UNPORTED();
}

void CreatureLifeStats::sendAttackStatusPacketUpdate(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value,
	int32_t skillId, std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log) {
	AION_UNPORTED();
}

void CreatureLifeStats::sendAttackStatusPacketUpdate(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value,
	int32_t skillId, std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log, bool criticalHit) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, gameobjects::Creature& effector) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, skillengine::model::Effect& effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, runtime::Ptr<gameobjects::Creature> effector,
	int32_t skillId, network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::increaseMp(int32_t value) {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::increaseMp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log) {
	AION_UNPORTED();
}

void CreatureLifeStats::restoreHp() {
	AION_UNPORTED();
}

void CreatureLifeStats::restoreMp() {
	AION_UNPORTED();
}

void CreatureLifeStats::triggerRestoreTask() {
	AION_UNPORTED();
}

void CreatureLifeStats::cancelRestoreTask() {
	AION_UNPORTED();
}

bool CreatureLifeStats::isFullyRestoredHpMp() {
	AION_UNPORTED();
}

bool CreatureLifeStats::isFullyRestoredHp() {
	AION_UNPORTED();
}

bool CreatureLifeStats::isFullyRestoredMp() {
	AION_UNPORTED();
}

void CreatureLifeStats::synchronizeWithMaxStats() {
	AION_UNPORTED();
}

void CreatureLifeStats::updateCurrentStats() {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::getHpPercentage() {
	AION_UNPORTED();
}

int32_t CreatureLifeStats::getMpPercentage() {
	AION_UNPORTED();
}

void CreatureLifeStats::onHpChanged(int32_t previousHp, int32_t newHp, runtime::Ptr<gameobjects::Creature> effector) {
	AION_UNPORTED();
}

void CreatureLifeStats::onMpChanged(int32_t previousMp, int32_t newMp) {
	AION_UNPORTED();
}

void CreatureLifeStats::cancelAllTasks() {
	AION_UNPORTED();
}

void CreatureLifeStats::setCurrentHpPercent(int32_t hpPercent) {
	AION_UNPORTED();
}

void CreatureLifeStats::setCurrentHp(int32_t hp) {
	AION_UNPORTED();
}

void CreatureLifeStats::setCurrentHp(int32_t hp, gameobjects::Creature& effector) {
	AION_UNPORTED();
}

void CreatureLifeStats::setCurrentMp(int32_t value) {
	AION_UNPORTED();
}

void CreatureLifeStats::setCurrentMpPercent(int32_t mpPercent) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::container
