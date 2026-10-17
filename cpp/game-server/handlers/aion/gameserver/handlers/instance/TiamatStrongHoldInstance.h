#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance {

/**
 * Tiamat Stronghold (300510000): one of four colonels (219364) among their three guards at the four corners; the gate's death (730612) sends
 * three waves of drakans at the players (the second after five drakan deaths, the third after twelve). Each brigade general's death tells
 * STR_IDTIAMAT_TIAMAT_REWARD_SPAWN, brings a reward box at Tiamat and opens its doors; Tahabata's (219352) also brings Kahrun's army marching in
 * 7 s later. With the three generals 219354, 219356 and 219358 dead the exit and its two npcs come. Laksyaka's HQ starts Surama's event once,
 * the Glorious Nexus takes effect 300 off; device 701494 opens door 22.
 * <p>
 * Java: data/handlers/instance/TiamatStrongHoldInstance.java.
 *
 * @author Cheatkiller
 */
class TiamatStrongHoldInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicInteger drakans{AION_LOCK_CLASS(TiamatStrongHoldInstance::drakans)}; // Java: = new AtomicInteger()
	runtime::AtomicBoolean startSuramaEvent{AION_LOCK_CLASS(TiamatStrongHoldInstance::startSuramaEvent)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> isInstanceDestroyed{};

protected:
	explicit TiamatStrongHoldInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<TiamatStrongHoldInstance> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;

private:
	void firstWave();

	void secondWave();

	void thirdWave();

	void attackPlayer(Npc& npc);

	void spawnKahrun();

	void moveToForward(Npc& npc, float x, float y, float z, bool despawn);

	void spawnColonels();

	void spawnExitIfCleared();

public:
	void onEnterZone(Player& player, ZoneInstance& zone) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onInstanceCreate() override;

	void onInstanceDestroy() override;

	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
