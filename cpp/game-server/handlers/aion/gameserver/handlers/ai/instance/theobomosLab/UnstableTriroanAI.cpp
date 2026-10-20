#include "aion/gameserver/handlers/ai/instance/theobomosLab/UnstableTriroanAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::theobomosLab {

AION_AI(UnstableTriroanAI, "triroan");

// Java UnstableTriroanAI.java:29-35
void UnstableTriroanAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (hpPhases->getCurrentPhase() > 0 && getLifeStats()->isFullyRestoredHp())
		hpPhases->reset();
	hpPhases->tryEnterNextPhase(*this);
}

// Java UnstableTriroanAI.java:37-69
void UnstableTriroanAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 99:
			SkillEngine::getInstance().getSkill(getOwner(), 16699, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
			break;
		case 90:
			spawnFire();
			break;
		case 80:
			spawnWater();
			break;
		case 70:
			spawnEarth();
			break;
		case 60:
			spawnWind();
			break;
		case 50:
			spawnFire();
			break;
		case 40:
			spawnFire();
			spawnWater();
			break;
		case 30:
			spawnEarth();
			spawnWind();
			break;
		case 20:
			spawnWind();
			spawnFire();
			break;
		case 10:
			spawnWater();
			spawnEarth();
			break;
		case 5:
			spawnWind();
			spawnFire();
			spawnWater();
			spawnEarth();
			break;
		default:
			break;
	}
}

// Java UnstableTriroanAI.java:71-73
void UnstableTriroanAI::spawnFire() {
	startWalk(*runtime::cast<Npc>(spawn(280975, 601.966f, 488.853f, 196.019f, int8_t{0})), "3101100002");
}

// Java UnstableTriroanAI.java:75-77
void UnstableTriroanAI::spawnWater() {
	startWalk(*runtime::cast<Npc>(spawn(280976, 601.966f, 488.853f, 196.019f, int8_t{0})), "3101100003");
}

// Java UnstableTriroanAI.java:79-81
void UnstableTriroanAI::spawnEarth() {
	startWalk(*runtime::cast<Npc>(spawn(280977, 601.966f, 488.853f, 196.019f, int8_t{0})), "3101100004");
}

// Java UnstableTriroanAI.java:83-85
void UnstableTriroanAI::spawnWind() {
	startWalk(*runtime::cast<Npc>(spawn(280978, 601.966f, 488.853f, 196.019f, int8_t{0})), "3101100005");
}

// Java UnstableTriroanAI.java:87-92
void UnstableTriroanAI::startWalk(Npc& npc, std::string_view walkId) {
	npc.getSpawn()->setWalkerId(walkId);
	WalkManager::startWalking(dynamic_cast<NpcAI&>(npc.getAi())); // Java: (NpcAI) npc.getAi()
	npc.setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(npc, SM_EMOTION(npc, EmotionType::CHANGE_SPEED, 0, npc.getObjectId()));
}

// Java UnstableTriroanAI.java:94-98
void UnstableTriroanAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::theobomosLab
