#include "aion/gameserver/handlers/ai/instance/darkPoeta/TelepathyControllerAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(TelepathyControllerAI, "telepathy_controller");

// Java TelepathyControllerAI.java:27-42 (Java's cast of the spawn to Npc: both ids are npcs)
void TelepathyControllerAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isAggred.compareAndSet(false, true)) {
		SYNCHRONIZED(isAggred.monitor()) {
			if (isAggred.get()) {
				spawnTask = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
					if (!isDead()) {
						runtime::Ptr<Npc> spawn = runtime::cast<Npc>(rndSpawnInRange(commons::utils::Rnd::nextInt(2) == 0 ? 281150 : 281334, 7, 10));
						spawn->getKnownList().forEachPlayer([&spawn](Player& p) { spawn->getAggroList().addHate(p, 10); });
					}
				}, 60000, 60000);
			}
		}
	}
}

// Java TelepathyControllerAI.java:44-47
void TelepathyControllerAI::cancelTask() {
	runtime::FutureRef task = spawnTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java TelepathyControllerAI.java:49-56
void TelepathyControllerAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	SYNCHRONIZED(isAggred.monitor()) {
		cancelTask();
		isAggred.set(false);
	}
}

// Java TelepathyControllerAI.java:58-62
void TelepathyControllerAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java TelepathyControllerAI.java:64-68
void TelepathyControllerAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
