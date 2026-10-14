#include "aion/gameserver/handlers/ai/instance/esoterrace/DaliaCharlandsAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

AION_AI(DaliaCharlandsAI, "dalia_charlands");

// Java DaliaCharlandsAI.java:29-35
void DaliaCharlandsAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (hpPhases->getCurrentPhase() > 0 && getLifeStats()->getHpPercentage() > 80)
		hpPhases->reset();
	else
		hpPhases->tryEnterNextPhase(*this);
}

// Java DaliaCharlandsAI.java:38-46
void DaliaCharlandsAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
		case 50:
		case 25:
			spawnHelpers();
			break;
		default:
			break;
	}
}

// Java DaliaCharlandsAI.java:48-52 (Java's casts of the spawns to Npc: the three ids are npcs)
void DaliaCharlandsAI::spawnHelpers() {
	startWalk(*runtime::cast<Npc>(spawn(282177, 1173.68f, 674.11f, 297.5f, int8_t{0})), "3002500001");
	startWalk(*runtime::cast<Npc>(spawn(282176, 1174.44f, 669.64f, 297.5f, int8_t{0})), "3002500002");
	startWalk(*runtime::cast<Npc>(spawn(282178, 1176.2f, 677.32f, 297.5f, int8_t{0})), "3002500003");
}

// Java DaliaCharlandsAI.java:54-59
void DaliaCharlandsAI::startWalk(Npc& npc, std::string_view walkId) {
	npc.getSpawn()->setWalkerId(walkId);
	WalkManager::startWalking(dynamic_cast<NpcAI&>(npc.getAi()));
	npc.setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(npc, network::aion::serverpackets::SM_EMOTION(npc, EmotionType::CHANGE_SPEED, 0, npc.getObjectId()));
}

// Java DaliaCharlandsAI.java:63-66
void DaliaCharlandsAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
