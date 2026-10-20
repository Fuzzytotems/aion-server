#pragma once

#include <string_view>

#include "aion/gameserver/handlers/instance/InstancePrelude.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Shugo Imperial Tomb (300560000), a three-stage defense: each stage list starts its stage once (1, 2, 3 in order) with defense towers, a
 * monument and waves of kobolds and kralls walking their routes, a bonus stage and two bosses (one of two or three routes each). A stage's
 * first boss starts its second phase; the last boss of stage 1 and 2 brings a delighted admirer (stage 2 also a bonus room), Captain Lediar
 * (stage 3) the Imperial Shrine, the chests and the exit portals. A destroyed monument ends the run (the disappointed admirer; the third also
 * the shrine, chests and portals). Entering players get the stage's transformation 1 s later; leaving sends them to the instance's exit.
 * <p>
 * Java: data/handlers/instance/ShugoImperialTombInstance.java. **Java bug, ported (proposed correction J-I6-3):** despawnNpcs lists 291512 and
 * 291513 for 219512 (Krall Lookout Commander) and 219513 (Swift Krall Graverobber), so a destroyed monument leaves those two standing; proposed:
 * the ids 219512 and 219513.
 *
 * @author Ritsu, Estrayl
 */
class ShugoImperialTombInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	/** Java: private final List<Future<?>> spawnTasks = new ArrayList<>() */
	runtime::ArrayList<runtime::FutureRef> spawnTasks{AION_LOCK_CLASS(ShugoImperialTombInstance::spawnTasks)};
	runtime::AtomicInteger stage{AION_LOCK_CLASS(ShugoImperialTombInstance::stage)}; // Java: = new AtomicInteger()

protected:
	explicit ShugoImperialTombInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<ShugoImperialTombInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onInstanceDestroy() override;

	void onEnterInstance(Player& player) override;

private:
	void applyTransformation(Player& player);

public:
	void onDie(Npc& npc) override;

	void onChangeStageList(StageList list) override;

private:
	void handleBossSpawn(StageList stageList);

	void startBonusStage1();

	void spawnBonusStage2();

	void startBonusStage3();

	void spawnExitPortals();

	void spawnChests();

	void sp(int32_t npcId, float x, float y, float z, std::string_view walkerId);

	void sp(int32_t npcId, float x, float y, float z, std::string_view walkerId, bool isRun);

public:
	void leaveInstance(Player& player) override;

private:
	void despawnNpcs();

	void cancelTasks();
};

} // namespace aion::gameserver::handlers::instance
