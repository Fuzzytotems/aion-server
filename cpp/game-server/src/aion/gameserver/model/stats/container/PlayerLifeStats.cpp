#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"

#include <cstdint>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FLY_TIME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_HP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_MP.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/LifeStatsRestoreService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::stats::container {

using LOG = network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using TYPE = network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

namespace {

/** Java int multiplication (wraps on overflow), like CreatureLifeStats' addInt/subtractInt */
constexpr int32_t multiplyInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

PlayerLifeStats::PlayerLifeStats(gameobjects::player::Player& ownerValue)
	: CreatureLifeStats(ownerValue, ownerValue.getGameStats()->getMaxHp()->getCurrent(), ownerValue.getGameStats()->getMaxMp()->getCurrent()),
	  currentFp(ownerValue.getGameStats()->getFlyTime()->getCurrent()) {
}

PlayerLifeStats::~PlayerLifeStats() = default;

void PlayerLifeStats::onHpChanged(int32_t previousHp, int32_t newHp, runtime::Ptr<gameobjects::Creature> effector) {
	gameobjects::player::Player& player = getOwner();
	if (isFullyRestoredHp()) // FIXME: Temp Fix: Reset aggro list when hp is full
		player.getAggroList().clear();
	if (player.isSpawned()) {
		sendHpPacketUpdate();
		sendGroupPacketUpdate();
		if (previousHp == 0 || newHp < previousHp)
			triggerRestoreTask();
		if (previousHp == 0)
			triggerFpRestore();
	}
	CreatureLifeStats::onHpChanged(previousHp, newHp, effector);
}

void PlayerLifeStats::onMpChanged(int32_t previousMp, int32_t newMp) {
	CreatureLifeStats::onMpChanged(previousMp, newMp);
	if (getOwner().isSpawned()) {
		sendMpPacketUpdate();
		sendGroupPacketUpdate();
		if (newMp < previousMp)
			triggerRestoreTask();
	}
}

void PlayerLifeStats::sendGroupPacketUpdate() {
	if (getOwner().isInTeam()) {
		// Java: TeamStatUpdater.getInstance().add(owner) - taskmanager/tasks/TeamStatUpdater.h (P5-10) does not exist yet; no team at M5a
		AION_UNPORTED();
	}
}

void PlayerLifeStats::synchronizeWithMaxStats() {
	if (isDead())
		return;

	CreatureLifeStats::synchronizeWithMaxStats();
	currentFp = getMaxFp();

	if (getOwner().isSpawned()) {
		sendHpPacketUpdate();
		sendMpPacketUpdate();
		sendFpPacketUpdate();
	}
}

void PlayerLifeStats::updateCurrentStats() {
	CreatureLifeStats::updateCurrentStats();

	if (!isFullyRestoredHpMp())
		triggerRestoreTask();

	// java-race: unsynchronized check-then-set like Java
	if (getMaxFp() < currentFp.get())
		currentFp = getMaxFp();

	if (getOwner().getFlyState() == 0 && !getOwner().isInSprintMode())
		triggerFpRestore();
}

void PlayerLifeStats::sendHpPacketUpdate() {
	utils::PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_STATUPDATE_HP(getCurrentHp(), getMaxHp()));
}

void PlayerLifeStats::sendMpPacketUpdate() {
	utils::PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_STATUPDATE_MP(getCurrentMp(), getMaxMp()));
}

int32_t PlayerLifeStats::getCurrentFp() {
	return currentFp.get();
}

int32_t PlayerLifeStats::getMaxFp() {
	return getOwner().getGameStats()->getFlyTime()->getCurrent();
}

int32_t PlayerLifeStats::getFpPercentage() {
	int32_t maxFp = getMaxFp();
	if (maxFp == 0) // Java: integer division by zero
		throw runtime::ArithmeticException("/ by zero");
	return multiplyInt(100, currentFp.get()) / maxFp;
}

int32_t PlayerLifeStats::increaseFp(TYPE type, int32_t value, int32_t skillId, LOG log) {
	SYNCHRONIZED(fpLock) {
		if (isDead()) {
			return 0;
		}
		int32_t newFp = this->currentFp.get() + value;
		if (newFp > getMaxFp()) {
			newFp = getMaxFp();
			value = getMaxFp() - this->currentFp.get();
		}
		if (currentFp.get() != newFp) {
			this->currentFp = newFp;
			onIncreaseFp(type, value, skillId, log);
		}
	}

	return currentFp.get();
}

int32_t PlayerLifeStats::reduceFp(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log) {
	SYNCHRONIZED(fpLock) {
		int32_t newFp = this->currentFp.get() - value;

		if (newFp < 0) {
			newFp = 0;
			value = this->currentFp.get();
		}

		this->currentFp = newFp;
	}

	onReduceFp(type, value, skillId, log);

	return currentFp.get();
}

int32_t PlayerLifeStats::setCurrentFp(int32_t value) {
	SYNCHRONIZED(fpLock) {
		int32_t newFp = value;

		if (newFp < 0)
			newFp = 0;

		this->currentFp = newFp;
	}

	onReduceFp(std::nullopt, value, 0, std::nullopt);

	return currentFp.get();
}

void PlayerLifeStats::onIncreaseFp(TYPE type, int32_t value, int32_t skillId, LOG log) {
	if (value > 0) {
		sendAttackStatusPacketUpdate(type, value, skillId, log);
		sendFpPacketUpdate();
	}
}

void PlayerLifeStats::onReduceFp(std::optional<TYPE> type, int32_t value, int32_t skillId, std::optional<LOG> log) {
	sendAttackStatusPacketUpdate(type, value, skillId, log);
	sendFpPacketUpdate();
}

void PlayerLifeStats::sendFpPacketUpdate() {
	utils::PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_FLY_TIME(currentFp.get(), getMaxFp()));
}

void PlayerLifeStats::restoreFp() {
	// how much fly time restoring per 6 second.
	increaseFp(TYPE::NATURAL_FP, 3, 0, LOG::REGULAR);
}

void PlayerLifeStats::specialrestoreFp() {
	PlayerGameStats& gameStats = *getOwner().getGameStats();
	if (gameStats.getStat(StatEnum::REGEN_FP, 0)->getCurrent() != 0)
		increaseFp(TYPE::NATURAL_FP, gameStats.getStat(StatEnum::REGEN_FP, 0)->getCurrent() / 3, 0, LOG::REGULAR);
}

void PlayerLifeStats::triggerFpRestore() {
	SYNCHRONIZED(restoreLock) {
		cancelFpReduce();
		// lockdep: flyRestoreTask.get() reads the Field<FutureRef>; nothing waits for the task
		if (!flyRestoreTask.get() && !isDead() && !isFlyTimeFullyRestored()) {
			flyRestoreTask = services::LifeStatsRestoreService::getInstance().scheduleFpRestoreTask(*this);
		}
	}
}

void PlayerLifeStats::cancelFpRestore() {
	SYNCHRONIZED(restoreLock) {
		// lockdep: flyRestoreTask.get() reads the Field<FutureRef>; cancel(false) does not wait for the task
		runtime::Ptr<runtime::Future> task = flyRestoreTask.get();
		if (task && !task->isCancelled()) {
			task->cancel(false);
			flyRestoreTask = nullptr;
		}
	}
}

void PlayerLifeStats::triggerFpReduce() {
	gameobjects::player::Player& player = getOwner();
	if (player.hasAccess(configs::administration::AdminConfig::UNLIMITED_FLIGHT_TIME.load()) || isDead())
		return;
	SYNCHRONIZED(restoreLock) {
		if (player.isInSprintMode()) {
			const templates::ride::RideInfo* ride = player.ride.get();
			if (ride == nullptr || !ride->getCostFp()) // Java: owner.ride.getCostFp() unboxed
				throw runtime::NullPointerException("ride cost_fp is null");
			flightReduceValue = *ride->getCostFp();
			flightReducePeriod = 1;
		} else if (player.isFlying()) {
			bool isInFlyArea = player.isInsideZoneType(templates::zone::ZoneType::FLY) && !player.isInsideZoneType(templates::zone::ZoneType::NO_FLY);
			flightReduceValue = isInFlyArea ? 1 : 2;
			flightReducePeriod = isInFlyArea && player.isInGlidingState() ? 2 : 1;
		} else {
			return;
		}
		cancelFpRestore();
		// lockdep: flyReduceTask.get() reads the Field<FutureRef>; nothing waits for the task
		if (!flyReduceTask.get() && !isDead())
			flyReduceTask = services::LifeStatsRestoreService::getInstance().scheduleFpReduceTask(*this);
	}
}

void PlayerLifeStats::cancelFpReduce() {
	SYNCHRONIZED(restoreLock) {
		// lockdep: flyReduceTask.get() reads the Field<FutureRef>; cancel(false) does not wait for the task
		runtime::Ptr<runtime::Future> task = flyReduceTask.get();
		if (task && !task->isCancelled()) {
			task->cancel(false);
			flyReduceTask = nullptr;
		}
	}
}

bool PlayerLifeStats::isFlyTimeFullyRestored() {
	return getMaxFp() == currentFp.get();
}

void PlayerLifeStats::cancelAllTasks() {
	CreatureLifeStats::cancelAllTasks();
	cancelFpReduce();
	cancelFpRestore();
}

gameobjects::player::Player& PlayerLifeStats::getOwner() const {
	return static_cast<gameobjects::player::Player&>(CreatureLifeStats::getOwner());
}

} // namespace aion::gameserver::model::stats::container
