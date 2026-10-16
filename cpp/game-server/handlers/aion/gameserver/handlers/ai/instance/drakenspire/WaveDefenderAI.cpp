#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveDefenderAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(WaveDefenderAI, "wave_defender");

// Java WaveDefenderAI.java:24-29
void WaveDefenderAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	if (getOwner().getMoveController()->isStop() && isDestinationReached.compareAndSet(false, true))
		scheduleLocationUpdate();
}

// Java WaveDefenderAI.java:31-36
void WaveDefenderAI::scheduleLocationUpdate() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(getOwner().getRace() == Race::ELYOS ? 236248 : 236249, getOwner().getX(), getOwner().getY(), getOwner().getZ(), getHeading());
		getOwner().getController().delete_();
	}, 3000);
}

// Java WaveDefenderAI.java:38-50
int8_t WaveDefenderAI::getHeading() {
	const std::string walkerId = getOwner().getSpawn()->getWalkerId().value(); // Java: a switch on the String (null throws)
	if (walkerId == "301390000_NPCPathFunction_Npc_Path07")
		return 100;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path08")
		return 110;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path09")
		return 3;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path10")
		return 10;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path13")
		return 77;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path14")
		return 64;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path15")
		return 64;
	if (walkerId == "301390000_NPCPathFunction_Npc_Path16")
		return 55;
	return 30;
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
