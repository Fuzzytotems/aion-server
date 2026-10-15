#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableGatesSummonedAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableGatesSummonedAI, "summonedunstablelapilima");

// Java UnstableGatesSummonedAI.java:31-35
void UnstableGatesSummonedAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startMove();
}

// Java UnstableGatesSummonedAI.java:37-40
bool UnstableGatesSummonedAI::canThink() {
	return canThink_.get();
}

// Java UnstableGatesSummonedAI.java:42-46
void UnstableGatesSummonedAI::handleDied() {
	cancelEventTask();
	GeneralNpcAI::handleDied();
}

// Java UnstableGatesSummonedAI.java:48-52
void UnstableGatesSummonedAI::handleDespawned() {
	cancelEventTask();
	GeneralNpcAI::handleDespawned();
}

// Java UnstableGatesSummonedAI.java:54-58
void UnstableGatesSummonedAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	startEventTask();
}

// Java UnstableGatesSummonedAI.java:60-67 (AIActions.targetCreature(this, null) when Yamennes is gone: the owner's target is cleared)
void UnstableGatesSummonedAI::startMove() {
	canThink_ = false;
	EmoteManager::emoteStopAttacking(getOwner());
	setStateIfNot(AIState::FOLLOWING);
	getOwner().setState(CreatureState::ACTIVE, true);
	runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(219563);
	if (boss != nullptr)
		AIActions::targetCreature(*this, *boss);
	else
		getOwner().setTarget(nullptr);
	getMoveController()->moveToTargetObject();
}

// Java UnstableGatesSummonedAI.java:69-72
void UnstableGatesSummonedAI::cancelEventTask() {
	runtime::FutureRef task = eventTask.get();
	if (task != nullptr && !task->isDone())
		task->cancel(true);
}

// Java UnstableGatesSummonedAI.java:74-83
void UnstableGatesSummonedAI::startEventTask() {
	eventTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(219563);
		if (boss == nullptr || boss->isDead()) {
			cancelEventTask();
			return;
		}
		SkillEngine::getInstance().getSkill(getOwner(), commons::utils::Rnd::nextBoolean() ? 19257 : 19281, 55, boss)->useNoAnimationSkill();
	}, 5000, 30000));
}

// Java UnstableGatesSummonedAI.java:85-91
bool UnstableGatesSummonedAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
