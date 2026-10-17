#include "aion/gameserver/handlers/ai/worlds/inggison/SematariuxEggAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(SematariuxEggAI, "sematariux_egg");

// Java SematariuxEggAI.java:24-29
void SematariuxEggAI::handleSpawned() {
	NpcAI::handleSpawned();
	spawnTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(281456, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
	}, 4, runtime::TimeUnit::MINUTES));
}

// Java SematariuxEggAI.java:31-40
void SematariuxEggAI::handleDied() {
	runtime::Ptr<Npc> sematariux = getOwner().getWorldMapInstance()->getNpc(216520);
	if (sematariux != nullptr) {
		sematariux->getEffectController()->removeEffect(18726);
		sematariux->queueSkill(19199, 1, 3000);
	}
	cancelSpawnTask();
	NpcAI::handleDied();
}

// Java SematariuxEggAI.java:42-46
void SematariuxEggAI::handleDespawned() {
	cancelSpawnTask();
	NpcAI::handleDespawned();
}

// Java SematariuxEggAI.java:48-51
void SematariuxEggAI::cancelSpawnTask() {
	runtime::FutureRef task = spawnTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java SematariuxEggAI.java:53-59
bool SematariuxEggAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
