#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombAttackerAI.h"

#include <algorithm>
#include <vector>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

AION_AI(ShugoTombAttackerAI, "shugo_tomb_attacker");

// Java ShugoTombAttackerAI.java:32-35
bool ShugoTombAttackerAI::canThink() {
	return canThink_.get();
}

// Java ShugoTombAttackerAI.java:37-41
void ShugoTombAttackerAI::handleSpawned() {
	canThink_ = false;
	GeneralNpcAI::handleSpawned();
}

// Java ShugoTombAttackerAI.java:43-52
void ShugoTombAttackerAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	if (getOwner().getMoveController()->getCurrentStep()->isLastStep() && isDestinationReached() && isWalkerComplete.compareAndSet(false, true)) {
		getSpawnTemplate()->setWalkerId(std::nullopt);
		WalkManager::stopWalking(*this);
		canThink_ = true;
		handleHate();
	}
}

// Java ShugoTombAttackerAI.java:54-70 (Java goes on after deleteOwner when no tower is near: the loop has nothing to do)
void ShugoTombAttackerAI::handleHate() {
	EmoteManager::emoteStopAttacking(getOwner());
	std::vector<runtime::Ptr<Npc>> towers;
	for (int32_t id : npc_ids) {
		for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs({id})) {
			if (npc != nullptr && !npc->isDead() && PositionUtil::isInRange(getOwner(), *npc, 30))
				towers.push_back(npc);
		}
	}

	if (towers.empty())
		AIActions::deleteOwner(*this);

	std::stable_sort(towers.begin(), towers.end(), [this](const runtime::Ptr<Npc>& a, const runtime::Ptr<Npc>& b) {
		return PositionUtil::getDistance(getOwner(), *a) < PositionUtil::getDistance(getOwner(), *b);
	});

	int32_t hateToAdd = 1000000;
	for (const runtime::Ptr<Npc>& tower : towers) {
		getOwner().getAggroList().addHate(*tower, hateToAdd);
		hateToAdd -= 50000;
	}
}

// Java ShugoTombAttackerAI.java:72-78
bool ShugoTombAttackerAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
