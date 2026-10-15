#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/GatesSummonedAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(GatesSummonedAI, "gatessummoned");

// Java GatesSummonedAI.java:31-35
void GatesSummonedAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startMove();
}

// Java GatesSummonedAI.java:37-40
bool GatesSummonedAI::canThink() {
	return canThink_.get();
}

// Java GatesSummonedAI.java:42-46
void GatesSummonedAI::handleDied() {
	cancelEventTask();
	GeneralNpcAI::handleDied();
}

// Java GatesSummonedAI.java:48-52
void GatesSummonedAI::handleDespawned() {
	cancelEventTask();
	GeneralNpcAI::handleDespawned();
}

// Java GatesSummonedAI.java:54-58
void GatesSummonedAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	startEventTask();
}

// Java GatesSummonedAI.java:60-67 (AIActions.targetCreature(this, null) when Yamennes is gone: the owner's target is cleared)
void GatesSummonedAI::startMove() {
	canThink_ = false;
	EmoteManager::emoteStopAttacking(getOwner());
	setStateIfNot(AIState::FOLLOWING);
	getOwner().setState(CreatureState::ACTIVE, true);
	runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(216960);
	if (boss != nullptr)
		AIActions::targetCreature(*this, *boss);
	else
		getOwner().setTarget(nullptr);
	getMoveController()->moveToTargetObject();
}

// Java GatesSummonedAI.java:69-72
void GatesSummonedAI::cancelEventTask() {
	runtime::FutureRef task = eventTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java GatesSummonedAI.java:74-83
void GatesSummonedAI::startEventTask() {
	eventTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(216960);
		if (boss == nullptr || boss->isDead()) {
			cancelEventTask();
			return;
		}
		SkillEngine::getInstance().getSkill(getOwner(), commons::utils::Rnd::nextBoolean() ? 19257 : 19281, 55, boss)->useNoAnimationSkill();
	}, 5000, 30000));
}

// Java GatesSummonedAI.java:85-91
bool GatesSummonedAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
