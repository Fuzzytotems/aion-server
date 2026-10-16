#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::instance {

/**
 * Stonespear Reach (301500000), the Legion Dominion instance: three minutes of preparation, then four stages of waves around the guardian
 * stone in the centre (attackers, aetheric fields whose survival ends the run, a kebbit, a boss per stage), the general of illusion last; the
 * score ranks S to F at the general's death, a stone's death, the aetheric field timeout, every member dead or the 30 minute limit, and the
 * legion's score goes to LegionDominionService.onFinishInstance.
 * <p>
 * Java: data/handlers/instance/StonespearReachInstance.java, @InstanceID(301500000). C++: the scheduled lambdas are pinned to the handler;
 * Java's synchronized methods (checkRank, canEnter) hold the monitor member.
 *
 * @author Yeats
 */
class StonespearReachInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<LegionDominionScore>> reward{};
	runtime::Field<std::optional<int64_t>> startTime{}; // Java: Long, null until onInstanceCreate
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(StonespearReachInstance::tasks)}; // Java: = new ArrayList<>()
	runtime::ArrayList<runtime::Ref<WorldPosition>> points{AION_LOCK_CLASS(StonespearReachInstance::points)}; // Java: = new ArrayList<>()
	runtime::Field<runtime::FutureRef> timer{};
	runtime::Field<runtime::FutureRef> failTask{};
	runtime::Field<std::optional<Race>> instanceRace{}; // fieldmap: Java null until the first entry; a nullable enum (hub-headers.md §6)
	runtime::Field<runtime::Ref<Legion>> instanceLegion{};
	runtime::Field<int32_t> aethericKills{};
	runtime::ArrayList<int32_t> allowedPlayers{AION_LOCK_CLASS(StonespearReachInstance::allowedPlayers)}; // Java: = new ArrayList<>()
	runtime::ArrayList<int32_t> deadPlayers{AION_LOCK_CLASS(StonespearReachInstance::deadPlayers)}; // Java: = new ArrayList<>()
	runtime::Monitor monitor{AION_LOCK_CLASS(StonespearReachInstance::monitor)}; // fieldmap: Java's synchronized (this)

protected:
	explicit StonespearReachInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<StonespearReachInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onEnterInstance(Player& player) override;

	void onDie(Npc& npc) override;

private:
	void checkRank(int32_t rankPoints, bool bossKilled);

	void reward_();

	void startInstance();

	void startStage1_1();
	void startStage1_2();
	void startStage1_3();
	void startStage1_4();
	void startStage2_1();
	void startStage2_2();
	void startStage2_3();
	void startStage2_4();
	void startStage3_1();
	void startStage3_2();
	void startStage3_3();
	void startStage3_4();
	void startStage3_5();
	void startStage4_1();
	void startStage4_2();
	void startStage4_3();
	void startStage4_4();
	void startStage4_5();
	void startStage4_6();

	void startFailTask();

	void addPoints(Npc& npc, int32_t value);

	void sendPacket(std::optional<std::string_view> npcL10n, int32_t value);

	int32_t getTime();

	void spawnAtPointsTask(int32_t delay, int32_t npcId, int32_t index);

	using GeneralInstanceHandler::spawn;

	void spawn(int32_t npcId, int32_t index);

	void rndSpawnTask(int32_t delay, int32_t npcId, int32_t amount, int32_t minRange, int32_t maxRange);

	void rndSpawnInRange(int32_t npcId, int32_t amount, int32_t minRange, int32_t maxRange);

	void spawnAtRandomPositionWithinRange(int32_t npcId, int32_t minRange, int32_t maxRange);

	void spawnGuardianStone(bool isElyos, int32_t stage);

	bool isValidPoint(float x, float y);

	void addWorldPoints();

	void despawnAll();

	void cancelAllTasks();

	/** Java: tasks.add(ThreadPoolManager.getInstance().schedule(runnable, delay)) */
	void addTask(std::function<void()> runnable, int32_t delay);

public:
	bool onDie(Player& player, Creature& lastAttacker) override;

	bool onReviveEvent(Player& player) override;

	void onInstanceDestroy() override;

private:
	bool allPlayersAreDead();

	void checkInstance();

public:
	bool canEnter(Player& p) override;

	void leaveInstance(Player& player) override;

private:
	void startAethericFieldTask(int32_t stage);

	/** Java: npcId = base + Rnd.get(0, 8) */
	static int32_t rndNpcId(int32_t base);
};

} // namespace aion::gameserver::handlers::instance
