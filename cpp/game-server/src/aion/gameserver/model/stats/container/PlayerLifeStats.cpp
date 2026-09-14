#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"

namespace aion::gameserver::model::stats::container {

PlayerLifeStats::PlayerLifeStats(gameobjects::player::Player& ownerValue)
	: CreatureLifeStats(ownerValue, ownerValue.getGameStats()->getMaxHp()->getCurrent(), ownerValue.getGameStats()->getMaxMp()->getCurrent()),
	  currentFp(ownerValue.getGameStats()->getFlyTime()->getCurrent()) {
}

PlayerLifeStats::~PlayerLifeStats() = default;

void PlayerLifeStats::onHpChanged(int32_t previousHp, int32_t newHp, runtime::Ptr<gameobjects::Creature> effector) {
	AION_UNPORTED();
}

void PlayerLifeStats::onMpChanged(int32_t previousMp, int32_t newMp) {
	AION_UNPORTED();
}

void PlayerLifeStats::sendGroupPacketUpdate() {
	AION_UNPORTED();
}

void PlayerLifeStats::synchronizeWithMaxStats() {
	AION_UNPORTED();
}

void PlayerLifeStats::updateCurrentStats() {
	AION_UNPORTED();
}

void PlayerLifeStats::sendHpPacketUpdate() {
	AION_UNPORTED();
}

void PlayerLifeStats::sendMpPacketUpdate() {
	AION_UNPORTED();
}

int32_t PlayerLifeStats::getCurrentFp() {
	return currentFp.get();
}

int32_t PlayerLifeStats::getMaxFp() {
	AION_UNPORTED();
}

int32_t PlayerLifeStats::getFpPercentage() {
	AION_UNPORTED();
}

int32_t PlayerLifeStats::increaseFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, int32_t skillId,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) {
	AION_UNPORTED();
}

int32_t PlayerLifeStats::reduceFp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log) {
	AION_UNPORTED();
}

int32_t PlayerLifeStats::setCurrentFp(int32_t value) {
	AION_UNPORTED();
}

void PlayerLifeStats::onIncreaseFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, int32_t skillId,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) {
	AION_UNPORTED();
}

void PlayerLifeStats::onReduceFp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
	std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log) {
	AION_UNPORTED();
}

void PlayerLifeStats::sendFpPacketUpdate() {
	AION_UNPORTED();
}

void PlayerLifeStats::restoreFp() {
	AION_UNPORTED();
}

void PlayerLifeStats::specialrestoreFp() {
	AION_UNPORTED();
}

void PlayerLifeStats::triggerFpRestore() {
	AION_UNPORTED();
}

void PlayerLifeStats::cancelFpRestore() {
	AION_UNPORTED();
}

void PlayerLifeStats::triggerFpReduce() {
	AION_UNPORTED();
}

void PlayerLifeStats::cancelFpReduce() {
	AION_UNPORTED();
}

bool PlayerLifeStats::isFlyTimeFullyRestored() {
	AION_UNPORTED();
}

void PlayerLifeStats::cancelAllTasks() {
	AION_UNPORTED();
}

gameobjects::player::Player& PlayerLifeStats::getOwner() const {
	return static_cast<gameobjects::player::Player&>(CreatureLifeStats::getOwner());
}

} // namespace aion::gameserver::model::stats::container
