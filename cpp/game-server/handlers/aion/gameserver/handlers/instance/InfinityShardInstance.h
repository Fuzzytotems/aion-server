#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Infinity Shard (300800000): each of four elites' death takes 21371 (Ide Aetheric Field) off its Ide Forcefield Generator (231074, 231078,
 * 231082, 231086); a generator's death deletes it and, while one stands, tells STR_MSG_IDRUNEWP_BROKENPROTECTION. With the last one the barrier
 * (730741) goes, Hyperion (231073) loses 21254, hates the nearest living player, casts 21255, the resonators (231092-231095, 10 s apart) and four
 * turrets (231102, at 30-45 s) come, and a 20-minute fail timer starts. A dead resonator or turret comes back after 29-31 s. Hyperion's effects
 * 21258, 21382, 21384 and 21416 tell the charges; the fourth fails the run 12 s later (231104 casts 21199). Hyperion's death or his return home
 * (once) despawns everything and opens the exit (730842). A player logging out dead is sent to his bind point.
 * <p>
 * Java: data/handlers/instance/InfinityShardInstance.java.
 *
 * @author Cheatkiller, Luzien, Estrayl
 */
class InfinityShardInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	/** Java: private final List<Future<?>> tasks = new ArrayList<>() */
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(InfinityShardInstance::tasks)};
	runtime::AtomicBoolean isRunning{AION_LOCK_CLASS(InfinityShardInstance::isRunning), true}; // Java: = new AtomicBoolean(true)

protected:
	explicit InfinityShardInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<InfinityShardInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;

private:
	void removeIdeAethericFieldEffect(int32_t npcId);

public:
	void onBackHome(Npc& npc) override;

	void onStartEffect(runtime::Ptr<Effect> effect) override;

private:
	void checkGeneratorState();

	void startHatingNearestPlayer();

	void startFailTimer();

	void failInstance(bool wipePlayers);

	void spawnResonators();

	void spawnTurrets();

	void despawnAllAndSpawnExit();

	void cancelTasks();

public:
	void onPlayerLogout(Player& player) override;

	void onInstanceDestroy() override;

	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
