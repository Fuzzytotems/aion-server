#include "aion/gameserver/handlers/ai/events/WorldRaidAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(WorldRaidAI, "world_raid_aggressive");

// Java WorldRaidAI.java:28-41. The despawn lambda (WorldRaidAI.java:32) pins this AI (and so its npc) until it has run or was cancelled.
void WorldRaidAI::handleAttack(runtime::Ptr<Creature> creature) {
	if (isEffectNpcSpawned.compareAndSet(false, true)) {
		// spawn npc 702549 (WorldRaid_Advent_Effect)
		effectNpc.set(runtime::cast<Npc>(spawn(702549, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
		effectNpcDespawnTask.set(ThreadPoolManager::getInstance().schedule(this, [this] {
			if (runtime::Ptr<Npc> npc = effectNpc.get()) {
				npc->getController().delete_();
				effectNpc.set(nullptr);
			}
			effectNpcDespawnTask.set(nullptr);
		}, 10000));
	}
	AggressiveNpcAI::handleAttack(creature);
}

// Java WorldRaidAI.java:44-47
void WorldRaidAI::handleBackHome() {
	cancelDespawnTaskAndDespawnEffectNpc();
	AggressiveNpcAI::handleBackHome();
}

// Java WorldRaidAI.java:50-53
void WorldRaidAI::handleDespawned() {
	cancelDespawnTaskAndDespawnEffectNpc();
	AggressiveNpcAI::handleDespawned();
}

// Java WorldRaidAI.java:56-59
void WorldRaidAI::handleDied() {
	cancelDespawnTaskAndDespawnEffectNpc();
	AggressiveNpcAI::handleDied();
}

// Java WorldRaidAI.java:61-71
void WorldRaidAI::cancelDespawnTaskAndDespawnEffectNpc() {
	runtime::Ptr<runtime::Future> task = effectNpcDespawnTask.get();
	if (task && !task->isCancelled()) {
		task->cancel(true);
		effectNpcDespawnTask.set(nullptr);
	}
	if (runtime::Ptr<Npc> npc = effectNpc.get()) {
		npc->getController().delete_();
		effectNpc.set(nullptr);
	}
	isEffectNpcSpawned.set(false);
}

} // namespace aion::gameserver::handlers::ai::events
