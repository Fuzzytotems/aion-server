#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The life stats part of Player (Player::postConstruct, after the game stats). The
 * constructor reads max HP/MP and the fly time from the owner's game stats (member stores only) and is ported. `fpLock` is the Monitor that
 * replaces `new Object()`. reduceFp and onReduceFp take `std::optional` type and log (LifeStatsRestoreService and setCurrentFp pass null, "no
 * attack status packet").
 *
 * @author ATracer, sphinx
 */
class PlayerLifeStats : public CreatureLifeStats {
private:
	runtime::Monitor fpLock{AION_LOCK_CLASS(PlayerLifeStats::fpLock)};
	runtime::Field<int32_t> flightReducePeriod{2};
	runtime::Field<int32_t> flightReduceValue{1};
	runtime::Field<int32_t> currentFp{};
	runtime::Field<runtime::FutureRef> flyRestoreTask{};
	runtime::Field<runtime::FutureRef> flyReduceTask{};

public:
	explicit PlayerLifeStats(gameobjects::player::Player& owner);
	~PlayerLifeStats() override;

	/** Narrowing accessor (Java CreatureLifeStats<Player>.getOwner(), hub-headers.md §8.2) */
	gameobjects::player::Player& getOwner() const;

protected:
	void onHpChanged(int32_t previousHp, int32_t newHp, runtime::Ptr<gameobjects::Creature> effector) override;

	void onMpChanged(int32_t previousMp, int32_t newMp) override;

private:
	void sendGroupPacketUpdate();

public:
	void synchronizeWithMaxStats() override;

	void updateCurrentStats() override;

private:
	void sendHpPacketUpdate();

	void sendMpPacketUpdate();

public:
	int32_t getCurrentFp() override;

	int32_t getMaxFp() override;

	int32_t getFpPercentage();

	/**
	 * This method is called whenever caller wants to restore creatures's FP
	 */
	int32_t increaseFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, int32_t skillId,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log); // synchronized (fpLock)

	/**
	 * This method is called whenever caller wants to reduce creatures's FP
	 */
	int32_t reduceFp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log); // synchronized (fpLock)

	int32_t setCurrentFp(int32_t value); // synchronized (fpLock)

protected:
	void onIncreaseFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t value, int32_t skillId,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log);

	void onReduceFp(std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_TYPE> type, int32_t value, int32_t skillId,
		std::optional<network::aion::serverpackets::SM_ATTACK_STATUS_LOG> log);

public:
	void sendFpPacketUpdate();

	/**
	 * this method should be used only on FlyTimeRestoreService
	 */
	void restoreFp();

	void specialrestoreFp();

	void triggerFpRestore(); // synchronized (restoreLock)

	void cancelFpRestore(); // synchronized (restoreLock)

	void triggerFpReduce(); // synchronized (restoreLock)

	void cancelFpReduce(); // synchronized (restoreLock)

	bool isFlyTimeFullyRestored();

	void cancelAllTasks() override;

	int32_t getFlightReducePeriod() const { return flightReducePeriod.get(); }

	int32_t getFlightReduceValue() const { return flightReduceValue.get(); }
};

} // namespace aion::gameserver::model::stats::container
