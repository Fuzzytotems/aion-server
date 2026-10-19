#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiFlamelordAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(CalindiFlamelordAI, "IDTiamat_2_calindi_flamelord");

// Java CalindiFlamelordAI.java:25-29
void CalindiFlamelordAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java CalindiFlamelordAI.java:31-36
void CalindiFlamelordAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	blazeEngraving();
	hpPhases->tryEnterNextPhase(*this);
}

// Java CalindiFlamelordAI.java:38-44
void CalindiFlamelordAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
		case 50:
		case 25:
			startHallucinatoryVictoryEvent();
			break;
		case 12:
			getOwner().queueSkill(20942, 1);
			break;
	}
}

// Java CalindiFlamelordAI.java:46-49
void CalindiFlamelordAI::startHallucinatoryVictoryEvent() {
	if (getPosition()->getWorldMapInstance()->getNpc(730695) == nullptr && getPosition()->getWorldMapInstance()->getNpc(730696) == nullptr)
		getOwner().queueSkill(20911, 1);
}

// Java CalindiFlamelordAI.java:51-64
void CalindiFlamelordAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20911:
			spawn(730695, 482.21f, 458.06f, 427.42f, int8_t{98});
			spawn(730696, 482.21f, 571.16f, 427.42f, int8_t{22});
			rndSpawn(283133);
			break;
		case 20913: {
			runtime::Ptr<Creature> target = getRandomTarget();
			if (target != nullptr)
				spawn(283131, target->getX(), target->getY(), target->getZ(), int8_t{0});
		}
	}
}

// Java CalindiFlamelordAI.java:66-69
void CalindiFlamelordAI::blazeEngraving() {
	if (commons::utils::Rnd::chance() < 2 && getPosition()->getWorldMapInstance()->getNpc(283131) == nullptr)
		getOwner().queueSkill(20913, 60);
}

// Java CalindiFlamelordAI.java:71-75
void CalindiFlamelordAI::rndSpawn(int32_t npcId) {
	for (int32_t i = 0; i < 10; i++) {
		rndSpawnInRange(npcId, 5, 20);
	}
}

// Java CalindiFlamelordAI.java:77-79
runtime::Ptr<Creature> CalindiFlamelordAI::getRandomTarget() {
	return getAggroList().getTarget(AggroTarget::RANDOM, 50);
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
