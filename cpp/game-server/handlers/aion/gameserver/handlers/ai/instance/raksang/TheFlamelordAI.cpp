#include "aion/gameserver/handlers/ai/instance/raksang/TheFlamelordAI.h"

#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(TheFlamelordAI, "the_flamelord");

// Java TheFlamelordAI.java:30-36
void TheFlamelordAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 90:
			startPhaseTask();
			break;
		case 40:
		case 30:
		case 20:
		case 10:
			startPhaseEvent(phaseHpPercent);
			break;
		default:
			break;
	}
}

// Java TheFlamelordAI.java:38-74 (the anonymous Runnable: a lambda pinned to the AI)
void TheFlamelordAI::startPhaseEvent(const int32_t percent) {
	cancelPhaseTask();
	PacketSendUtility::broadcastMessage(getOwner(), 1401120);
	SkillEngine::getInstance().getSkill(getOwner(), 19980, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	ThreadPoolManager::getInstance().schedule({this}, [this, percent] {
		if (!isDead()) {
			switch (percent) {
				case 40:
					moveExecutor(282451);
					break;
				case 30:
					moveExecutor(282451);
					moveExecutor(282452);
					break;
				case 20:
					moveExecutor(282451);
					moveExecutor(282452);
					moveExecutor(282453);
					break;
				case 10:
					moveExecutor(282451);
					moveExecutor(282452);
					moveExecutor(282453);
					moveExecutor(282454);
					break;
				default:
					break;
			}
			SkillEngine::getInstance().getSkill(getOwner(), 19924, 44, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			cancelPhaseTask();
			startPhaseTask();
		}
	}, 5000);
}

// Java TheFlamelordAI.java:76-107 (the task keeps the executor alive, as Java's capture does)
void TheFlamelordAI::moveExecutor(const int32_t executorId) {
	const runtime::Ref<Npc> npc(*runtime::cast<Npc>(spawn(executorId, 802.845f, 964.903f, 792.102f, int8_t{0})));
	ThreadPoolManager::getInstance().schedule({this}, [this, executorId, npc] {
		if (!isDead()) {
			int32_t targetId = 0;
			switch (executorId) {
				case 282451:
					targetId = 701062;
					break;
				case 282452:
					targetId = 701063;
					break;
				case 282453:
					targetId = 701064;
					break;
				case 282454:
					targetId = 701065;
					break;
				default:
					break;
			}
			runtime::Ptr<Npc> target = getPosition()->getWorldMapInstance()->getNpc(targetId);
			if (target != nullptr) {
				npc->setTarget(target);
				npc->getMoveController()->moveToTargetObject();
			}
		}
	}, 1500);
}

// Java TheFlamelordAI.java:109-123 (the anonymous Runnable: a lambda pinned to the AI)
void TheFlamelordAI::startPhaseTask() {
	phaseTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelPhaseTask();
		} else {
			SkillEngine::getInstance().getSkill(getOwner(), 19925, 44, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			PacketSendUtility::broadcastMessage(getOwner(), 1401119);
		}
	}, 3000, 30000));
}

// Java TheFlamelordAI.java:125-129
void TheFlamelordAI::cancelPhaseTask() {
	runtime::FutureRef task = phaseTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java TheFlamelordAI.java:131-137
void TheFlamelordAI::handleDied() {
	cancelPhaseTask();
	getPosition()->getWorldMapInstance()->setDoorState(118, true);
	PacketSendUtility::broadcastMessage(getOwner(), 1401121);
	AggressiveNpcAI::handleDied();
}

// Java TheFlamelordAI.java:139-143
void TheFlamelordAI::handleDespawned() {
	cancelPhaseTask();
	AggressiveNpcAI::handleDespawned();
}

// Java TheFlamelordAI.java:145-152
void TheFlamelordAI::handleAttack(runtime::Ptr<Creature> creature) {
	if (isAggred.compareAndSet(false, true)) {
		PacketSendUtility::broadcastMessage(getOwner(), 1401118);
	}
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java TheFlamelordAI.java:154-160
void TheFlamelordAI::handleBackHome() {
	cancelPhaseTask();
	isAggred.set(false);
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
