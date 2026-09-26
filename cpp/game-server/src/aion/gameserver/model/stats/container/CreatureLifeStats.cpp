#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LifeStatsRestoreService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::stats::container {

using network::aion::serverpackets::SM_ATTACK_STATUS;
using LOG = network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

namespace {

/** Java int subtraction (wraps on overflow) */
constexpr int32_t subtractInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java int addition (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

CreatureLifeStats::CreatureLifeStats(gameobjects::Creature& value, int32_t currentHpValue, int32_t currentMpValue)
	: runtime::OwnedPart(value), currentHp(currentHpValue), currentMp(currentMpValue), owner(value) {
}

CreatureLifeStats::~CreatureLifeStats() = default;

int32_t CreatureLifeStats::getMaxHp() {
	return getOwner().getGameStats()->getMaxHp()->getCurrent();
}

int32_t CreatureLifeStats::getMaxMp() {
	return getOwner().getGameStats()->getMaxMp()->getCurrent();
}

bool CreatureLifeStats::isDead() {
	return currentHp.get() == 0;
}

bool CreatureLifeStats::isAboutToDie() {
	return killingBlow.get() != 0;
}

void CreatureLifeStats::unsetIsAboutToDie() {
	this->killingBlow = 0;
}

int32_t CreatureLifeStats::reduceHp(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log, gameobjects::Creature& attacker) {
	return reduceHp(type, value, skillId, log, attacker, false);
}

int32_t CreatureLifeStats::reduceHp(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log, gameobjects::Creature& attacker,
	bool criticalHit) {
	// Java: Objects.requireNonNull(attacker, "attacker") - a reference is never null
	if (getOwner().isInvulnerable()) {
		unsetIsAboutToDie();
		return currentHp.get();
	}

	int32_t previousHp = 0;
	int32_t newHp = 0;
	SYNCHRONIZED(*this) {
		if (isDead())
			return 0;

		previousHp = currentHp.get();
		int32_t minHp = type == TYPE::USED_HP ? 1 : 0; // a skill cost drains its caster down to 1 hp, it never kills him
		// Java: Math.clamp(currentHp - value, minHp, currentHp) (int subtraction, then clamp(long, int, int))
		if (minHp > previousHp)
			throw runtime::IllegalArgumentException(std::to_string(minHp) + " > " + std::to_string(previousHp));
		newHp = std::clamp(subtractInt(previousHp, value), minHp, previousHp);
		currentHp = newHp;
		if (isDead()) {
			currentMp = 0;
			unsetIsAboutToDie();
		}
	}

	if (newHp != previousHp || skillId != 0)
		sendAttackStatusPacketUpdate(type, subtractInt(previousHp, newHp), skillId, log, criticalHit);
	if (newHp != previousHp)
		onHpChanged(previousHp, newHp, attacker);
	return newHp;
}

int32_t CreatureLifeStats::reduceMp(TYPE type, int32_t value, int32_t skillId, LOG log) {
	int32_t previousMp = 0;
	int32_t newMp = 0;
	SYNCHRONIZED(*this) {
		if (isDead())
			return 0;

		previousMp = currentMp.get();
		newMp = std::min(previousMp, std::max(subtractInt(previousMp, value), 0));
		currentMp = newMp;
	}

	if (newMp != previousMp || skillId != 0)
		sendAttackStatusPacketUpdate(type, subtractInt(previousMp, newMp), skillId, log);
	if (newMp != previousMp)
		onMpChanged(previousMp, newMp);
	return newMp;
}

void CreatureLifeStats::sendAttackStatusPacketUpdate(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log) {
	sendAttackStatusPacketUpdate(type, value, skillId, log, false);
}

void CreatureLifeStats::sendAttackStatusPacketUpdate(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log, bool criticalHit) {
	if (type) {
		// Java passes a null LOG with a non-null TYPE nowhere (the null TYPE of increaseMp(int) comes with a null LOG); SM_ATTACK_STATUS reads
		// log.getId(), a NullPointerException for null
		if (!log)
			throw runtime::NullPointerException("SM_ATTACK_STATUS log is null");
		utils::PacketSendUtility::broadcastToSightedPlayers(owner, SM_ATTACK_STATUS(owner, *type, skillId, value, *log, criticalHit), true);
	}
}

int32_t CreatureLifeStats::increaseHp(TYPE type, int32_t value) {
	return increaseHp(type, value, runtime::Ptr<gameobjects::Creature>(getOwner()), 0, LOG::REGULAR);
}

int32_t CreatureLifeStats::increaseHp(TYPE type, int32_t value, gameobjects::Creature& effector) {
	return increaseHp(type, value, runtime::Ptr<gameobjects::Creature>(effector), 0, LOG::REGULAR);
}

int32_t CreatureLifeStats::increaseHp(TYPE type, int32_t value, skillengine::model::Effect& effect, LOG log) {
	return increaseHp(type, value, effect.getEffector(), effect.getSkillId(), log);
}

int32_t CreatureLifeStats::increaseHp(TYPE type, int32_t value, runtime::Ptr<gameobjects::Creature> effector, int32_t skillId, LOG log) {
	if (value < 0) { // some skills reduce hp via a negative heal (e.g. 3732 Spirit Absorption)
		if (!effector) // Java: Objects.requireNonNull(attacker, "attacker") in reduceHp
			throw runtime::NullPointerException("attacker");
		return reduceHp(type, subtractInt(0, value), skillId, log, *effector);
	}

	if (getOwner().getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::DISEASE))
		return currentHp.get();

	int32_t previousHp = 0;
	int32_t newHp = 0;
	SYNCHRONIZED(*this) {
		if (isDead())
			return 0;

		previousHp = currentHp.get();
		newHp = std::min(addInt(previousHp, value), getMaxHp());
		currentHp = newHp;
		if (killingBlow.get() != 0 && newHp > killingBlow.get())
			unsetIsAboutToDie();
	}

	if (newHp != previousHp || skillId != 0)
		sendAttackStatusPacketUpdate(type, subtractInt(newHp, previousHp), skillId, log);
	if (newHp != previousHp)
		onHpChanged(previousHp, newHp, effector ? effector : runtime::Ptr<gameobjects::Creature>(getOwner()));
	return newHp;
}

int32_t CreatureLifeStats::increaseMp(int32_t value) {
	return increaseMp(std::nullopt, value, 0, std::nullopt);
}

int32_t CreatureLifeStats::increaseMp(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log) {
	int32_t previousMp = 0;
	int32_t newMp = 0;
	SYNCHRONIZED(*this) {
		if (isDead())
			return 0;

		previousMp = currentMp.get();
		newMp = std::max(previousMp, std::min(addInt(previousMp, value), getMaxMp()));
		currentMp = newMp;
	}

	if (newMp != previousMp || skillId != 0)
		sendAttackStatusPacketUpdate(type, subtractInt(newMp, previousMp), skillId, log);
	if (newMp != previousMp)
		onMpChanged(previousMp, newMp);
	return currentMp.get();
}

void CreatureLifeStats::restoreHp() {
	increaseHp(TYPE::NATURAL_HP, getOwner().getGameStats()->getHpRegenRate()->getCurrent());
}

void CreatureLifeStats::restoreMp() {
	increaseMp(TYPE::NATURAL_MP, getOwner().getGameStats()->getMpRegenRate()->getCurrent(), 0, LOG::REGULAR);
}

void CreatureLifeStats::triggerRestoreTask() {
	SYNCHRONIZED(restoreLock) {
		// lockdep: lifeRestoreTask.get() reads the Field<FutureRef>; nothing waits for the task
		if (!lifeRestoreTask.get() && !isDead()) {
			lifeRestoreTask = services::LifeStatsRestoreService::getInstance().scheduleRestoreTask(*this);
		}
	}
}

void CreatureLifeStats::cancelRestoreTask() {
	SYNCHRONIZED(restoreLock) {
		// lockdep: lifeRestoreTask.get() reads the Field<FutureRef>; cancel(false) does not wait for the task
		if (runtime::Ptr<runtime::Future> task = lifeRestoreTask.get()) {
			task->cancel(false);
			lifeRestoreTask = nullptr;
		}
	}
}

bool CreatureLifeStats::isFullyRestoredHpMp() {
	return getMaxHp() == currentHp.get() && getMaxMp() == currentMp.get();
}

bool CreatureLifeStats::isFullyRestoredHp() {
	return getMaxHp() == currentHp.get();
}

bool CreatureLifeStats::isFullyRestoredMp() {
	return getMaxMp() == currentMp.get();
}

void CreatureLifeStats::synchronizeWithMaxStats() {
	// java-race: unsynchronized like Java (called on load and level up)
	currentHp = getMaxHp();
	currentMp = getMaxMp();
}

void CreatureLifeStats::updateCurrentStats() {
	// java-race: unsynchronized check-then-set like Java
	int32_t maxHp = getMaxHp();
	if (maxHp < currentHp.get())
		currentHp = maxHp;

	int32_t maxMp = getMaxMp();
	if (maxMp < currentMp.get())
		currentMp = maxMp;
}

int32_t CreatureLifeStats::getHpPercentage() {
	int32_t hp = currentHp.get();
	if (hp == 0)
		return 0;
	return std::max(1, templates::detail::floatToInt(100.0f * hp / getMaxHp()));
}

int32_t CreatureLifeStats::getMpPercentage() {
	return templates::detail::floatToInt(100.0f * currentMp.get() / getMaxMp());
}

void CreatureLifeStats::onHpChanged(int32_t previousHp, int32_t newHp, runtime::Ptr<gameobjects::Creature> effector) {
	if (newHp == 0) {
		if (!effector) // Java: onDie(null) reaches a dereference of the last attacker
			throw runtime::NullPointerException("effector");
		getOwner().getController().onDie(*effector);
	}
	getOwner().getObserveController()->notifyHPChangeObservers(newHp);
}

void CreatureLifeStats::onMpChanged(int32_t previousMp, int32_t newMp) {
}

void CreatureLifeStats::cancelAllTasks() {
	cancelRestoreTask();
}

void CreatureLifeStats::setCurrentHpPercent(int32_t hpPercent) {
	setCurrentHp(static_cast<int32_t>(static_cast<int64_t>(getMaxHp()) * hpPercent / 100));
}

void CreatureLifeStats::setCurrentHp(int32_t hp) {
	setCurrentHp(hp, owner);
}

void CreatureLifeStats::setCurrentHp(int32_t hp, gameobjects::Creature& effector) {
	int32_t previousHp = 0;
	int32_t newHp = 0;
	SYNCHRONIZED(*this) {
		previousHp = currentHp.get();
		newHp = std::max(0, std::min(hp, getMaxHp()));
		currentHp = newHp;
		if (killingBlow.get() != 0 && (newHp == 0 || newHp > killingBlow.get()))
			unsetIsAboutToDie();
	}
	if (newHp != previousHp) {
		// broadcast current hp percentage to others
		utils::PacketSendUtility::broadcastToSightedPlayers(owner, SM_ATTACK_STATUS(owner, TYPE::HP, 0, 0, LOG::REGULAR));
		onHpChanged(previousHp, newHp, effector);
	}
}

void CreatureLifeStats::setCurrentMp(int32_t value) {
	int32_t previousMp = 0;
	int32_t newMp = 0;
	SYNCHRONIZED(*this) {
		if (isDead())
			return;
		previousMp = currentMp.get();
		newMp = std::max(0, std::min(value, getMaxMp()));
		currentMp = newMp;
	}
	if (newMp != previousMp) {
		utils::PacketSendUtility::broadcastToSightedPlayers(owner, SM_ATTACK_STATUS(owner, TYPE::HEAL_MP, 0, 0, LOG::MPHEAL));
		onMpChanged(previousMp, newMp);
	}
}

void CreatureLifeStats::setCurrentMpPercent(int32_t mpPercent) {
	setCurrentMp(static_cast<int32_t>(static_cast<int64_t>(getMaxMp()) * mpPercent / 100));
}

} // namespace aion::gameserver::model::stats::container
