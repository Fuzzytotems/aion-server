#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Raksang Ruins (300610000): one of three ways chosen at creation picks the entry npc by race; the waves of the three trials (the forge's
 * three rounds near the player, the two arenas' eight rounds, the vault's seven rounds) open the doors; the bosses open theirs; the vault
 * door needs the trial; 1.8 times the experience.
 * <p>
 * Java: data/handlers/instance/RaksangRuinsInstance.java, @InstanceID(300610000). C++: the scheduled lambdas are pinned to the handler.
 */
class RaksangRuinsInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	const int32_t way;
	runtime::AtomicInteger spawns{AION_LOCK_CLASS(RaksangRuinsInstance::spawns)}; // Java: = new AtomicInteger()
	runtime::AtomicBoolean isEventStarted{AION_LOCK_CLASS(RaksangRuinsInstance::isEventStarted)}; // Java: = new AtomicBoolean()
	runtime::AtomicInteger waveKillz{AION_LOCK_CLASS(RaksangRuinsInstance::waveKillz)}; // Java: = new AtomicInteger()
	runtime::Field<bool> isInstanceDestroyed{}; // Java: volatile
	runtime::Field<runtime::FutureRef> spawnTask{};
	runtime::Field<bool> isDoorAccessible{false};

protected:
	explicit RaksangRuinsInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<RaksangRuinsInstance> create(WorldMapInstance& instance);

	float getExpMultiplier() override;

	void onEnterInstance(Player& player) override;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	/** Used for way A (Terror's Vault & Cuttling Grounds). */
	void delaySpawn(int32_t npcId, int32_t delay);

	void delaySpawn(int8_t step, int32_t delay);

	/** Used for way C (Torment's Forge). */
	void delaySpawn(Player& p, int32_t npcId, int32_t delay);

	void rndSpawnInRange(Player& player, int32_t npcId, int32_t distance);

public:
	void onCreatureDetected(Npc& detector, Creature& detected) override;

	void onEnterZone(Player& player, ZoneInstance& zone) override;

	void onInstanceDestroy() override;
};

} // namespace aion::gameserver::handlers::instance
