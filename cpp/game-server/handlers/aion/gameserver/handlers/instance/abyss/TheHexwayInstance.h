#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * The Hexway (300700000): the bonus chest ring starts 22 minutes to kill the six bridge bosses, each with its own time limit started by its
 * ring; a killed boss opens its treasure door and spawns its chests, all six in time spawn the bonus chest; the two halves of the second
 * bridge's boss.
 * <p>
 * Java: data/handlers/instance/abyss/TheHexwayInstance.java, @InstanceID(300700000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author xTz, Bobobear, Sykra
 */
class TheHexwayInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	static constexpr std::string_view MSG_TIMER_STARTED =
		"You have 22 minutes to kill all boss mobs located at the end of each bridge to unlock a bonus chest!";
	static constexpr std::string_view MSG_TIMER_NOTIFY = "You have %s left to unlock the bonus chest.";
	static constexpr std::string_view MSG_TIMER_EXPIRED = "Your time to unlock a bonus chest has expired!";
	static constexpr std::string_view MSG_BOSS_FAILED_NO_BOX = "You failed to kill a boss mob. No bonus chest will be unlocked.";
	static constexpr std::string_view MSG_BOSS_FAILED = "You failed to kill a boss mob.";
	static constexpr std::string_view MSG_SUCCESSFUL = "You have successfully completed the instance. A bonus chest was spawned.";
	const runtime::Ref<runtime::Array<int32_t>> bossTimeLimitsSeconds = runtime::Array<int32_t>::of({120, 210, 150, 150, 210, 120});
	const runtime::Ref<runtime::Array<int32_t>> treasureDoorIds = runtime::Array<int32_t>::of({60, 61, 63, 64, 65, 66});
	const runtime::Ref<runtime::Array<int32_t>> bossNpcIds = runtime::Array<int32_t>::of({219611, 286933, 219612, 219613, 219610, 219614});
	const int32_t bonusChestTimeLimitSeconds = 1320;
	const runtime::Ref<runtime::Array<int32_t>> notifyTimesSeconds = runtime::Array<int32_t>::of({900, 600, 300, 120, 60, 30, 10, 3, 2, 1});
	// Java: = new ScheduledFuture<?>[6]
	const runtime::Ref<runtime::Array<runtime::FutureRef>> scheduledBossDespawnTasks = runtime::Array<runtime::FutureRef>::make(6);
	runtime::AtomicLongArray stageStartMillis{AION_LOCK_CLASS(TheHexwayInstance::stageStartMillis), 6}; // Java: = new AtomicLongArray(6)
	runtime::ConcurrentHashMap<int32_t, int32_t> playerStageMapping{AION_LOCK_CLASS(TheHexwayInstance::playerStageMapping#stripe)};
	runtime::AtomicBoolean bonusChestSpawnAllowed{AION_LOCK_CLASS(TheHexwayInstance::bonusChestSpawnAllowed), true};
	runtime::AtomicLong bonusChestStartMillis{AION_LOCK_CLASS(TheHexwayInstance::bonusChestStartMillis)};
	runtime::ArrayList<runtime::FutureRef> timerProgressMsgTasks{AION_LOCK_CLASS(TheHexwayInstance::timerProgressMsgTasks)};
	runtime::AtomicInteger killedBossCount{AION_LOCK_CLASS(TheHexwayInstance::killedBossCount)};
	runtime::AtomicInteger attackedBossCount{AION_LOCK_CLASS(TheHexwayInstance::attackedBossCount)};
	runtime::Field<runtime::FutureRef> disableBonusChestSpawnTask{};
	runtime::AtomicBoolean handlingSecondBoss{AION_LOCK_CLASS(TheHexwayInstance::handlingSecondBoss)};

protected:
	explicit TheHexwayInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<TheHexwayInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	bool onPassFlyingRing(Player& player, std::string_view flyingRing) override;

	void onLeaveInstance(Player& player) override;

	void onInstanceDestroy() override;

	void onDie(Npc& npc) override;

	bool onReviveEvent(Player& player) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void cancelBossDespawn(int32_t bossIndex);

	void scheduleBossDespawn(int32_t bossIndex, int32_t despawnDelayMillis);

	void cancelTimeInformTasks();

	void spawnBonusChest();

	void spawnBossChest(int32_t bossIndex);

	void initTimerTrigger();

	void spawnBonusChestTimerTrigger();

	void spawnBossTimerTrigger(int32_t number, Point3D& p1, Point3D& center, Point3D& p2);

	void broadcastYellowMessage(std::string_view message);

	void sendTimeStringToPlayers(int32_t leftTimeSeconds);

	static std::string secondsToReadableString(int32_t remainingTimeSeconds);

	static std::string javaFormat(std::string_view pattern, std::string_view argument);
};

} // namespace aion::gameserver::handlers::instance::abyss
