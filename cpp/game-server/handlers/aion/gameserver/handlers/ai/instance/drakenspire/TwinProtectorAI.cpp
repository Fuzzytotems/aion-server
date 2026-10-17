#include "aion/gameserver/handlers/ai/instance/drakenspire/TwinProtectorAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(TwinProtectorAI, "twin_protector");

// Java TwinProtectorAI.java:28-32
void TwinProtectorAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNoLootNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java TwinProtectorAI.java:34-43
void TwinProtectorAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 65:
		case 40:
		case 15:
			getOwner().clearQueuedSkills();
			getOwner().queueSkill(21644, 1, 10000); // Raging Hellfire
			break;
		case 25:
		case 10:
			spawnAdds(getNpcId() % 2 == 0 ? 855622 : 855621, 20);
			break;
	}
}

// Java TwinProtectorAI.java:45-51
void TwinProtectorAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21644) { // Raging Hellfire
		SkillEngine::getInstance().getSkill(getOwner(), 21645, 1, getTarget())->useNoAnimationSkill();
		spawnAdds(855625, 50);
	}
}

// Java TwinProtectorAI.java:53-58
void TwinProtectorAI::spawnAdds(int32_t npcId, int32_t hpThreshold) {
	int32_t count = getLifeStats()->getHpPercentage() < hpThreshold ? 3 : 1;
	int32_t spawned = 0;
	for (const runtime::Ptr<Creature>& target : getAggroList().streamValidTargets(20)) {
		if (spawned++ >= count) // limit(count)
			break;
		adds.add(runtime::cast<Npc>(spawn(npcId, target->getX(), target->getY(), target->getZ(), int8_t{0})));
	}
}

// Java TwinProtectorAI.java:60-64
void TwinProtectorAI::despawnAdds() {
	for (const runtime::Ptr<Npc>& npc : adds)
		if (npc != nullptr)
			npc->getController().delete_();
}

// Java TwinProtectorAI.java:66-70
void TwinProtectorAI::handleDespawned() {
	despawnAdds();
	AggressiveNoLootNpcAI::handleDespawned();
}

// Java TwinProtectorAI.java:72-77
void TwinProtectorAI::handleBackHome() {
	despawnAdds();
	AggressiveNoLootNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
