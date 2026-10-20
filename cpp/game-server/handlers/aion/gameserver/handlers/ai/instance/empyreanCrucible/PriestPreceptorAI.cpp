#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/PriestPreceptorAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(PriestPreceptorAI, "priest_preceptor");

// Java PriestPreceptorAI.java:26-30
void PriestPreceptorAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19612, 15, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill(); }, 1000);
}

// Java PriestPreceptorAI.java:32-36
void PriestPreceptorAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java PriestPreceptorAI.java:38-42
void PriestPreceptorAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java PriestPreceptorAI.java:44-50
void PriestPreceptorAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19611, 10, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill();
			break;
		case 25:
			startEvent();
			break;
	}
}

// Java PriestPreceptorAI.java:52-73 (the anonymous Runnables are lambdas; Java's casts of the spawns to Npc: the three ids are npcs)
void PriestPreceptorAI::startEvent() {
	skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19610, 10, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19614, 10, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			runtime::Ptr<world::WorldPosition> p = getPosition();
			applySoulSickness(*runtime::cast<Npc>(spawn(282366, p->getX(), p->getY(), p->getZ(), p->getHeading())));
			applySoulSickness(*runtime::cast<Npc>(spawn(282367, p->getX(), p->getY(), p->getZ(), p->getHeading())));
			applySoulSickness(*runtime::cast<Npc>(spawn(282368, p->getX(), p->getY(), p->getZ(), p->getHeading())));
		}, 5000);
	}, 2000);
}

// Java PriestPreceptorAI.java:75-77
runtime::Ptr<Creature> PriestPreceptorAI::getRandomTarget() {
	return getAggroList().getTarget(AggroTarget::RANDOM, 25);
}

// Java PriestPreceptorAI.java:79-89 (the anonymous Runnable captures the npc)
void PriestPreceptorAI::applySoulSickness(Npc& npc) {
	ThreadPoolManager::getInstance().schedule({this}, [npc = runtime::Ref<Npc>(npc)] {
		npc->getLifeStats()->setCurrentHpPercent(50); // TODO: remove this, fix max hp debuffs not reducing current hp properly
		skillengine::SkillEngine::getInstance().getSkill(*npc, 19594, 4, runtime::Ptr<VisibleObject>(*npc))->useNoAnimationSkill();
	}, 1000);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
