#include "aion/gameserver/ai/manager/WalkManager.h"

#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::ai::manager {

namespace Rnd = commons::utils::Rnd;
using model::gameobjects::Npc;
using model::templates::walker::RouteStep;
using model::templates::walker::WalkerTemplate;
using utils::PositionUtil;

// Java: the anonymous Runnable at WalkManager.java:182 (WalkManager$1), scheduled by chooseNextRouteStep. A task object with only TaskArg
// members (fieldmap K3), ported as an aggregate TaskStruct value (runtime-architecture.md §7.3): the Ref retains the AI part and with it the
// npc, exactly as the Java capture does.
struct WalkManager_Runnable final : runtime::TaskStruct {
	const runtime::Ref<NpcAI> npcAI;

	void operator()() const {
		if (npcAI->isInState(AIState::WALKING)) {
			npcAI->getOwner().getMoveController()->moveToNextPoint();
		}
	}
};

// Java: the anonymous Runnable at WalkManager.java:202 (WalkManager$2), scheduled by chooseNextRandomPoint (fieldmap K3, see above).
struct WalkManager_Runnable_2 final : runtime::TaskStruct {
	const runtime::Ref<NpcAI> npcAI;
	const runtime::Ref<Npc> owner;

	void operator()() const {
		if (!npcAI->isInState(AIState::WALKING))
			return;
		int32_t randomWalkRange = owner->getSpawn()->getRandomWalkRange();
		int32_t diameter = randomWalkRange * 2;
		float nextX = Rnd::nextFloat(static_cast<float>(diameter)) - randomWalkRange + owner->getSpawn()->getX();
		float nextY = Rnd::nextFloat(static_cast<float>(diameter)) - randomWalkRange + owner->getSpawn()->getY();
		if (configs::main::GeoDataConfig::GEO_ENABLE.load() && configs::main::GeoDataConfig::GEO_NPC_MOVE.load()) {
			geoEngine::math::Vector3f loc = world::geo::GeoService::getInstance().getClosestCollision(*owner, nextX, nextY, owner->getZ(), true,
				WalkManager::RANDOM_WALK_GEO_FLAGS.get(), *geoEngine::collision::IgnoreProperties::of(owner->getRace()));
			owner->getMoveController()->moveToPoint(loc.x, loc.y, loc.z);
		} else {
			owner->getMoveController()->moveToPoint(nextX, nextY, owner->getZ());
		}
	}
};

bool WalkManager::startWalking(NpcAI& npcAI) {
	if (!configs::main::AIConfig::ACTIVE_NPC_MOVEMENT.load() || !npcAI.getOwner().isSpawned())
		return false;
	return startRandomWalking(npcAI) || startRouteWalking(npcAI);
}

bool WalkManager::startRandomWalking(NpcAI& npcAI) {
	if (!npcAI.getOwner().isRandomWalker())
		return false;
	if (!npcAI.setStateIfNot(AIState::WALKING) || !npcAI.setSubStateIfNot(AISubState::WALK_RANDOM))
		return false;
	EmoteManager::emoteStartWalking(npcAI.getOwner());
	chooseNextRandomPoint(npcAI);
	return true;
}

bool WalkManager::startRouteWalking(NpcAI& npcAI) {
	Npc& owner = npcAI.getOwner();
	if (!owner.isPathWalker())
		return false;
	if (owner.getMoveController()->getWalkerTemplate() == nullptr) {
		// Java: DataManager.WALKER_DATA.getWalkerTemplate(owner.getSpawn().getWalkerId()) - isPathWalker() is true, so the id is not null
		const WalkerTemplate* walkerTemplate = dataholders::DataManager::WALKER_DATA->getWalkerTemplate(*owner.getSpawn()->getWalkerId());
		if (walkerTemplate == nullptr)
			return false;
		owner.getMoveController()->setWalkerTemplate(walkerTemplate, 0);
	}
	if (!npcAI.setStateIfNot(AIState::WALKING) || !npcAI.setSubStateIfNot(AISubState::WALK_PATH))
		return false;
	const RouteStep* nextStep = findNextRoutStep(owner);
	owner.getMoveController()->setRouteStep(nextStep);
	EmoteManager::emoteStartWalking(npcAI.getOwner());
	npcAI.getOwner().getMoveController()->moveToNextPoint();
	return true;
}

void WalkManager::startForcedWalking(NpcAI& npcAI, float x, float y, float z) {
	npcAI.setStateIfNot(AIState::FORCED_WALKING);
	npcAI.setSubStateIfNot(AISubState::NONE);
	EmoteManager::emoteStartWalking(npcAI.getOwner());
	npcAI.getOwner().getMoveController()->forcedMoveToPoint(x, y, z);
}

const RouteStep* WalkManager::findNextRoutStep(Npc& owner) {
	const RouteStep* currentStep = owner.getMoveController()->getCurrentStep();
	const RouteStep* nextStep = nullptr;
	if (currentStep->getStepIndex() != 0) {
		nextStep = findNextRouteStepAfterPause(owner, *currentStep);
	} else {
		nextStep = findClosestRouteStep(owner);
	}
	return nextStep;
}

const RouteStep* WalkManager::findClosestRouteStep(Npc& owner) {
	const std::vector<std::unique_ptr<RouteStep>>& route = owner.getMoveController()->getWalkerTemplate()->getRouteSteps();
	const RouteStep* nextStep = nullptr;
	if (owner.getWalkerGroup()) {
		nextStep = route.at(static_cast<size_t>(owner.getWalkerGroup()->getGroupStep())).get();
	} else {
		double closestDist = 0;
		float x = owner.getX();
		float y = owner.getY();
		float z = owner.getZ();
		for (const std::unique_ptr<RouteStep>& step : route) {
			double stepDist = PositionUtil::getDistance(x, y, z, step->getX(), step->getY(), step->getZ());
			if (closestDist == 0 || stepDist < closestDist) {
				closestDist = stepDist;
				nextStep = step.get();
			}
		}
	}
	return nextStep;
}

const RouteStep* WalkManager::findNextRouteStepAfterPause(Npc& owner, const RouteStep& currentStep) {
	if (PositionUtil::isInRange(owner, currentStep.getX(), currentStep.getY(), currentStep.getZ(), 1)) {
		const std::vector<std::unique_ptr<RouteStep>>& route = owner.getMoveController()->getWalkerTemplate()->getRouteSteps();
		if (currentStep.isLastStep())
			return route.at(0).get();
		else
			return route.at(static_cast<size_t>(currentStep.getStepIndex() + 1)).get();
	}
	return &currentStep;
}

void WalkManager::targetReached(NpcAI& npcAI) {
	if (npcAI.isInState(AIState::WALKING)) {
		switch (npcAI.getSubState()) {
			case AISubState::WALK_PATH:
				npcAI.getOwner().updateKnownlist();
				if (npcAI.getOwner().getWalkerGroup()) {
					npcAI.getOwner().getWalkerGroup()->targetReached(npcAI);
				} else {
					chooseNextRouteStep(npcAI);
				}
				break;
			case AISubState::WALK_WAIT_GROUP:
				npcAI.setSubStateIfNot(AISubState::WALK_PATH);
				chooseNextRouteStep(npcAI);
				break;
			case AISubState::WALK_RANDOM:
				chooseNextRandomPoint(npcAI);
				break;
			case AISubState::TALK:
				npcAI.setStateIfNot(AIState::IDLE);
				npcAI.getOwner().getMoveController()->abortMove();
				break;
			default:
				break;
		}
	} else if (npcAI.isInState(AIState::FORCED_WALKING)) {
		npcAI.getOwner().getMoveController()->abortMove();
		npcAI.setStateIfNot(AIState::IDLE);
		npcAI.think();
	}
}

void WalkManager::chooseNextRouteStep(NpcAI& npcAI) {
	int32_t walkPause = npcAI.getOwner().getMoveController()->getCurrentStep()->getRestTime();
	if (walkPause == 0) {
		npcAI.getOwner().getMoveController()->resetMove();
		if (npcAI.getOwner().getMoveController()->isNextRouteStepChosen())
			npcAI.getOwner().getMoveController()->moveToNextPoint();
	} else {
		npcAI.getOwner().getMoveController()->abortMove();
		if (npcAI.getOwner().getMoveController()->isNextRouteStepChosen()) {
			utils::ThreadPoolManager::getInstance().schedule(WalkManager_Runnable{{}, runtime::Ref<NpcAI>(npcAI)}, walkPause);
		}
	}
}

void WalkManager::chooseNextRandomPoint(NpcAI& npcAI) {
	Npc& owner = npcAI.getOwner();
	owner.getMoveController()->abortMove();

	utils::ThreadPoolManager::getInstance().schedule(WalkManager_Runnable_2{{}, runtime::Ref<NpcAI>(npcAI), runtime::Ref<Npc>(owner)},
		Rnd::get(configs::main::AIConfig::MINIMIMUM_DELAY.load(), configs::main::AIConfig::MAXIMUM_DELAY.load()) * 1000);
}

void WalkManager::stopWalking(NpcAI& npcAI) {
	npcAI.getOwner().getMoveController()->abortMove();
	npcAI.setStateIfNot(AIState::IDLE);
	if (npcAI.getSubState() != AISubState::FREEZE)
		npcAI.setSubStateIfNot(AISubState::NONE);
	EmoteManager::emoteStopWalking(npcAI.getOwner());
}

bool WalkManager::isArrivedAtPoint(NpcAI& npcAI) {
	return npcAI.getOwner().getMoveController()->isReachedPoint();
}

} // namespace aion::gameserver::ai::manager
