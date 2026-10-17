#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/TraitorKumbandaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(TraitorKumbandaAI, "traitorkumbanda");

// Java TraitorKumbandaAI.java:28-39
void TraitorKumbandaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (commons::utils::Rnd::chance() < 5) {
		spawnTimeAccelerator();
		spawnKumbandaGhost();
	}
	if (!isFinalBuff.get() && getOwner().getLifeStats()->getHpPercentage() <= 5) {
		isFinalBuff = true;
		AIActions::useSkill(*this, 20942);
	}
}

// Java TraitorKumbandaAI.java:41-47
void TraitorKumbandaAI::spawnTimeAccelerator() {
	if (getPosition()->getWorldMapInstance()->getNpc(283086) == nullptr) {
		SkillEngine::getInstance().getSkill(getOwner(), 20726, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		spawn(283086, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
		rndSpawn(283086, 6);
	}
}

// Java TraitorKumbandaAI.java:49-53
void TraitorKumbandaAI::spawnKumbandaGhost() {
	if (getPosition()->getWorldMapInstance()->getNpc(283085) == nullptr && getOwner().getLifeStats()->getHpPercentage() <= 50) {
		spawn(283085, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
	}
}

// Java TraitorKumbandaAI.java:55-61
void TraitorKumbandaAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

// Java TraitorKumbandaAI.java:63-69
void TraitorKumbandaAI::handleDied() {
	AggressiveNpcAI::handleDied();
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	deleteNpcs(instance->getNpcs({283086}));
	deleteNpcs(instance->getNpcs({283088}));
}

// Java TraitorKumbandaAI.java:71-75
void TraitorKumbandaAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	isFinalBuff = false;
}

// Java TraitorKumbandaAI.java:77-80
void TraitorKumbandaAI::rndSpawn(int32_t npcId, int32_t count) {
	for (int32_t i = 0; i < count; i++)
		rndSpawnInRange(npcId, 10, 20);
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
