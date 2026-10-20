#include "aion/gameserver/handlers/ai/quests/NidalberBalaurAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(NidalberBalaurAI, "nidalber_balaur");

// Java NidalberBalaurAI.java:22-26
void NidalberBalaurAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	findQuestNpc();
}

// Java NidalberBalaurAI.java:28-32
void NidalberBalaurAI::setNewSpawnPosition() {
	getSpawnTemplate()->setX(271.153f);
	getSpawnTemplate()->setY(178.903f);
	getSpawnTemplate()->setZ(204.52f);
}

// Java NidalberBalaurAI.java:34-45
void NidalberBalaurAI::findQuestNpc() {
	int32_t mapId = getOwner().getPosition()->getMapId();
	if (mapId != 310040000 && mapId != 320040000)
		return;
	questNpc.set(getOwner().getPosition()->getWorldMapInstance()->getNpc(mapId == 310040000 ? 204044 : 204432));
	runtime::Ptr<Npc> npc = questNpc.get();
	if (npc != nullptr && !npc->isDead()) {
		setNewSpawnPosition();
		moveToQuestNpc();
	} else {
		getOwner().getController().delete_();
	}
}

// Java NidalberBalaurAI.java:47-51
void NidalberBalaurAI::moveToQuestNpc() {
	setStateIfNot(AIState::WALKING);
	getOwner().setState(CreatureState::ACTIVE, true);
	getOwner().getMoveController()->moveToPoint(getSpawnTemplate()->getX(), getSpawnTemplate()->getY(), getSpawnTemplate()->getZ());
}

// Java NidalberBalaurAI.java:53-59
void NidalberBalaurAI::handleNotAtHome() {
	if (getOwner().getPosition()->getMapId() != 310040000 && getOwner().getPosition()->getMapId() != 320040000)
		AggressiveNpcAI::handleNotAtHome();
	else
		moveToQuestNpc();
}

// Java NidalberBalaurAI.java:61-65
void NidalberBalaurAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	addHate();
}

// Java NidalberBalaurAI.java:67-72
void NidalberBalaurAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	if (getOwner().isAtSpawnLocation())
		addHate();
}

// Java NidalberBalaurAI.java:74-83
void NidalberBalaurAI::addHate() {
	if (getOwner().getPosition()->getMapId() != 310040000 && getOwner().getPosition()->getMapId() != 320040000)
		return;
	runtime::Ptr<Npc> npc = questNpc.get();
	if (npc == nullptr || npc->isDead()) {
		getOwner().getController().delete_();
		return;
	}
	if (!getAggroList().isHating(*npc))
		getAggroList().addHate(*npc, 100);
}

} // namespace aion::gameserver::handlers::ai::quests
