#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstablePazuzuAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstablePazuzuAI, "unstablepazuzu");

// Java UnstablePazuzuAI.java:29-36
void UnstablePazuzuAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		PacketSendUtility::broadcastMessage(getOwner(), 342219);
		startTask();
	}
}

// Java UnstablePazuzuAI.java:38-43
void UnstablePazuzuAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	cancelTask();
	isHome.set(true);
}

// Java UnstablePazuzuAI.java:45-50
void UnstablePazuzuAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
	PacketSendUtility::broadcastMessage(getOwner(), 1500003);
}

// Java UnstablePazuzuAI.java:52-62
void UnstablePazuzuAI::startTask() {
	task.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		SkillEngine::getInstance().getSkill(getOwner(), 19145, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		if (getPosition()->getWorldMapInstance()->getNpc(283206) == nullptr) {
			spawn(283206, 651.351990f, 326.425995f, 465.523987f, int8_t{8});
			spawn(283206, 666.604980f, 314.497009f, 465.394012f, int8_t{27});
			spawn(283206, 685.588989f, 342.955994f, 465.908997f, int8_t{68});
			spawn(283206, 651.322021f, 346.554993f, 465.563995f, int8_t{111});
		}
	}, 5000, 70000));
}

// Java UnstablePazuzuAI.java:64-68
void UnstablePazuzuAI::cancelTask() {
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isCancelled()) {
		current->cancel(true);
	}
}

// Java UnstablePazuzuAI.java:70-74
void UnstablePazuzuAI::handleDespawned() {
	cancelTask();
	AggressiveNpcAI::handleDespawned();
}

// Java UnstablePazuzuAI.java:76-82
bool UnstablePazuzuAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
