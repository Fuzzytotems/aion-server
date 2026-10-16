#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/MagePreceptorAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(MagePreceptorAI, "mage_preceptor");

// Java MagePreceptorAI.java:26-30
void MagePreceptorAI::handleDespawned() {
	despawnNpcs();
	AggressiveNpcAI::handleDespawned();
}

// Java MagePreceptorAI.java:32-36
void MagePreceptorAI::handleDied() {
	despawnNpcs();
	AggressiveNpcAI::handleDied();
}

// Java MagePreceptorAI.java:38-43
void MagePreceptorAI::handleBackHome() {
	despawnNpcs();
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java MagePreceptorAI.java:45-49
void MagePreceptorAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java MagePreceptorAI.java:51-78
void MagePreceptorAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 75:
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19605, 10, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill();
			break;
		case 50:
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19606, 10, getTarget())->useNoAnimationSkill();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (!isDead()) {
					skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19609, 10, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
					ThreadPoolManager::getInstance().schedule({this}, [this] {
						runtime::Ptr<world::WorldPosition> p = getPosition();
						spawn(282364, p->getX(), p->getY(), p->getZ(), p->getHeading());
						spawn(282363, p->getX(), p->getY(), p->getZ(), p->getHeading());
						scheduleSkill(2000);
					}, 4500);
				}
			}, 3000);
			break;
		case 25:
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19606, 10, getTarget())->useNoAnimationSkill();
			scheduleSkill(3000);
			scheduleSkill(9000);
			scheduleSkill(15000);
			break;
	}
}

// Java MagePreceptorAI.java:80-86
void MagePreceptorAI::scheduleSkill(int32_t delay) {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19605, 10, runtime::Ptr<VisibleObject>(getRandomTarget()))->useNoAnimationSkill();
		}
	}, delay);
}

// Java MagePreceptorAI.java:88-90
runtime::Ptr<Creature> MagePreceptorAI::getRandomTarget() {
	return getAggroList().getTarget(AggroTarget::RANDOM, 37);
}

// Java MagePreceptorAI.java:92-95
void MagePreceptorAI::despawnNpcs() {
	despawnNpc(getPosition()->getWorldMapInstance()->getNpc(282364));
	despawnNpc(getPosition()->getWorldMapInstance()->getNpc(282363));
}

// Java MagePreceptorAI.java:97-101
void MagePreceptorAI::despawnNpc(runtime::Ptr<Npc> npc) {
	if (npc != nullptr) {
		npc->getController().delete_();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
