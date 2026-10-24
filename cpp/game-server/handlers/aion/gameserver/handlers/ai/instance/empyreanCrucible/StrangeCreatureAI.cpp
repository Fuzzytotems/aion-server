#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/StrangeCreatureAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(StrangeCreatureAI, "strange_creature");

// Java StrangeCreatureAI.java:22-27
void StrangeCreatureAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startEventTask();
	startLifeTask();
}

// Java StrangeCreatureAI.java:29-41 (the anonymous Runnable is a lambda)
void StrangeCreatureAI::startEventTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 341444);
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 17914, 34, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		}
	}, 500);
}

// Java StrangeCreatureAI.java:43-52 (the anonymous Runnable is a lambda)
void StrangeCreatureAI::startLifeTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, 6500);
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
