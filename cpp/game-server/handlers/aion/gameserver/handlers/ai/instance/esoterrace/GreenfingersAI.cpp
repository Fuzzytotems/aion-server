#include "aion/gameserver/handlers/ai/instance/esoterrace/GreenfingersAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

AION_AI(GreenfingersAI, "greenfingers");

// Java GreenfingersAI.java:30-32
bool GreenfingersAI::canThink() {
	return false;
}

// Java GreenfingersAI.java:35-51
void GreenfingersAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	switch (getNpcId()) {
		case 282176:
			walkPosition = 24;
			helperSkill = 19271;
			break;
		case 282177:
			walkPosition = 26;
			helperSkill = 18751;
			break;
		case 282178:
			walkPosition = 40;
			helperSkill = 16634;
			break;
		default:
			break;
	}
}

// Java GreenfingersAI.java:54-68
void GreenfingersAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	const model::templates::walker::RouteStep* step = getOwner().getMoveController()->getCurrentStep();
	if (step == nullptr) // Java: getCurrentStep().getStepIndex() of no step
		throw runtime::NullPointerException("NpcMoveController.getCurrentStep()");
	int32_t point = step->getStepIndex();
	if (walkPosition.get() == point) {
		if (isDestroyed.compareAndSet(false, true)) {
			getSpawnTemplate()->setWalkerId(std::nullopt);
			WalkManager::stopWalking(*this);
			runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(217185);
			if (boss != nullptr) {
				skillengine::SkillEngine::getInstance().getSkill(getOwner(), helperSkill.get(), 55, runtime::Ptr<VisibleObject>(boss))->useNoAnimationSkill();
			}
			startDespawnTask();
		}
	}
}

// Java GreenfingersAI.java:70-75
void GreenfingersAI::startDespawnTask() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			AIActions::deleteOwner(*this);
	}, 3000);
}

// Java GreenfingersAI.java:78-84
bool GreenfingersAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
