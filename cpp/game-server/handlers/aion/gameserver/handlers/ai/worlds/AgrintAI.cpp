#include "aion/gameserver/handlers/ai/worlds/AgrintAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(AgrintAI, "agrint");

// Java AgrintAI.java:26-35
void AgrintAI::handleSpawned() {
	OneDmgAI::handleSpawned();
	switch (getNpcId()) {
		case 218862:
		case 218850:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_HF_SpringAgrintAppear());
			break;
		case 218863:
		case 218851:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_HF_SummerAgrintAppear());
			break;
		case 218864:
		case 218852:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_HF_FallAgrintAppear());
			break;
		case 218865:
		case 218853:
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_HF_WinterAgrintAppear());
			break;
		default:
			break;
	}
}

// Java AgrintAI.java:37-41
void AgrintAI::handleAttack(runtime::Ptr<Creature> creature) {
	OneDmgAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java AgrintAI.java:43-64
void AgrintAI::handleHpPhase(int32_t phaseHpPercent) {
	static_cast<void>(phaseHpPercent);
	switch (getNpcId()) {
		case 218850:
		case 218851:
		case 218852:
		case 218853: {
			int32_t npcId = getNpcId() + 320;
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			break;
		}
		case 218862:
		case 218863:
		case 218864:
		case 218865: {
			int32_t npcId = getNpcId() + 308;
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			rndSpawnInRange(npcId, 1, 2);
			break;
		}
		default:
			break;
	}
}

// Java AgrintAI.java:66-70
void AgrintAI::handleBackHome() {
	OneDmgAI::handleBackHome();
	hpPhases->reset();
}

// Java AgrintAI.java:72-79
void AgrintAI::spawnChests(int32_t npcId) {
	rndSpawnInRange(npcId, 1, 6);
	rndSpawnInRange(npcId, 1, 6);
	rndSpawnInRange(npcId, 1, 6);
	rndSpawnInRange(npcId, 1, 6);
	rndSpawnInRange(npcId, 1, 6);
	rndSpawnInRange(npcId, 1, 6);
}

// Java AgrintAI.java:81-94
void AgrintAI::handleDied() {
	switch (getNpcId()) {
		case 218850:
			spawnChests(218874);
			break;
		case 218851:
			spawnChests(218876);
			break;
		case 218852:
			spawnChests(218878);
			break;
		case 218853:
			spawnChests(218880);
			break;
		case 218862:
			spawnChests(218882);
			break;
		case 218863:
			spawnChests(218884);
			break;
		case 218864:
			spawnChests(218886);
			break;
		case 218865:
			spawnChests(218888);
			break;
		default:
			break;
	}
	OneDmgAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::worlds
