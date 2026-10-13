#pragma once

#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The boss of a world raid ("world_raid_aggressive"): its first attack spawns the advent effect npc 702549 for 10 seconds; going home,
 * despawning or dying removes it early.
 * <p>
 * Java: data/handlers/ai/events/WorldRaidAI.java, @AIName("world_raid_aggressive") (the marker is in the .cpp). M5i I-5, under P5-12b2's lease.
 *
 * @author Sykra
 */
class WorldRaidAI : public AggressiveNpcAI {
	runtime::AtomicBoolean isEffectNpcSpawned{AION_LOCK_CLASS(WorldRaidAI::isEffectNpcSpawned)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::Ref<Npc>> effectNpc{};
	runtime::Field<runtime::FutureRef> effectNpcDespawnTask{};

public:
	explicit WorldRaidAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;

private:
	void cancelDespawnTaskAndDespawnEffectNpc();
};

} // namespace aion::gameserver::handlers::ai::events
