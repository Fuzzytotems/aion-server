#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAggressiveNpcAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/templates/spawns/panesterra/AhserionsFlightSpawnTemplate.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionAggressiveNpcAI, "ahserion_aggressive_npc");

// Java AhserionAggressiveNpcAI.java:24-31
void AhserionAggressiveNpcAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	if (getNpcId() == 277242) {
		getOwner().getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
			getOwner().getController().deleteIfAliveOrCancelRespawn();
		}, 8, runtime::TimeUnit::MINUTES));
	}
}

// Java AhserionAggressiveNpcAI.java:33-35 (AggroList.addHate of a null target does nothing: isAware(null) is false)
void AhserionAggressiveNpcAI::addHateToRndTarget() {
	runtime::Ptr<Creature> target = getAggroList().getTarget(AggroTarget::RANDOM);
	if (target != nullptr)
		getAggroList().addHate(*target, 100000);
}

// Java AhserionAggressiveNpcAI.java:37-40
runtime::Ptr<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate> AhserionAggressiveNpcAI::getSpawnTemplate() {
	return runtime::cast<model::templates::spawns::panesterra::AhserionsFlightSpawnTemplate>(AggressiveNoLootNpcAI::getSpawnTemplate());
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
