#include "aion/gameserver/handlers/ai/classNpc/DrakanPriestAI.h"

#include <cmath>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::classNpc {

AION_AI(DrakanPriestAI, "xdrakanpriest");

// Java DrakanPriestAI.java:28-34
void DrakanPriestAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (commons::utils::Rnd::chance() < 3) {
		spawnServants(282988, commons::utils::Rnd::get(1, 3));
	}
}

// Java DrakanPriestAI.java:36-42
void DrakanPriestAI::spawnServants(int32_t npcId, int32_t count) {
	const std::vector<runtime::Ptr<model::gameobjects::Servant>> servants = findServants();
	if (servants.empty()) {
		rndSpawn(npcId, count);
		PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 341784);
	}
}

// Java DrakanPriestAI.java:44-48
void DrakanPriestAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	despawnServants();
}

// Java DrakanPriestAI.java:50-54
void DrakanPriestAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	despawnServants();
}

// Java DrakanPriestAI.java:56-60
void DrakanPriestAI::handleDied() {
	AggressiveNpcAI::handleDied();
	despawnServants();
}

// Java DrakanPriestAI.java:62-64
void DrakanPriestAI::despawnServants() {
	for (const runtime::Ptr<model::gameobjects::Servant>& servant : findServants())
		servant->getController().deleteIfAliveOrCancelRespawn();
}

// Java DrakanPriestAI.java:66-74
std::vector<runtime::Ptr<model::gameobjects::Servant>> DrakanPriestAI::findServants() {
	std::vector<runtime::Ptr<model::gameobjects::Servant>> servants;
	getPosition()->getWorldMapInstance()->forEachNpc([this, &servants](Npc& npc) {
		if (runtime::Ptr<model::gameobjects::Servant> servant = runtime::as<model::gameobjects::Servant>(runtime::Ptr<Npc>(npc))) {
			if (servant->getCreator().get() == &getOwner()) {
				servants.push_back(servant);
			}
		}
	});
	return servants;
}

// Java DrakanPriestAI.java:76-81
void DrakanPriestAI::rndSpawn(int32_t npcId, int32_t count) {
	for (int32_t i = 0; i < count; i++) {
		runtime::Ref<SpawnTemplate> spawnTemplate = rndSpawnInRange(npcId);
		spawnengine::VisibleObjectSpawner::spawnEnemyServant(*spawnTemplate, getOwner().getInstanceId(), getOwner(),
			static_cast<int8_t>(getOwner().getLevel()));
	}
}

// Java DrakanPriestAI.java:83-89 (Math.toRadians: times DEGREES_TO_RADIANS; cos and sin in double, then the float casts)
runtime::Ref<SpawnTemplate> DrakanPriestAI::rndSpawnInRange(int32_t npcId) {
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) * 0.017453292519943295; // Math.toRadians
	float x1 = static_cast<float>(std::cos(angleRadians) * 5);
	float y1 = static_cast<float>(std::sin(angleRadians) * 5);
	return spawnengine::SpawnEngine::newSingleTimeSpawn(getPosition()->getMapId(), npcId, getPosition()->getX() + x1, getPosition()->getY() + y1,
		getPosition()->getZ(), getPosition()->getHeading());
}

} // namespace aion::gameserver::handlers::ai::classNpc
