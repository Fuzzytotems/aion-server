#include "aion/gameserver/handlers/ai/siege/SpringAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(SpringAI, "spring");

// Java SpringAI.java:27-31
void SpringAI::handleSpawned() {
	NpcAI::handleSpawned();
	healCheckTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { checkForHeal(); }, 5000, 5000));
}

// Java SpringAI.java:33-37
void SpringAI::handleDied() {
	NpcAI::handleDied();
	cancelTasks();
}

// Java SpringAI.java:39-43
void SpringAI::handleDespawned() {
	NpcAI::handleDespawned();
	cancelTasks();
}

// Java SpringAI.java:45-51
bool SpringAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

// Java SpringAI.java:53-58
void SpringAI::cancelTasks() {
	runtime::FutureRef task = healCheckTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
		healCheckTask.set(nullptr);
	}
}

// Java SpringAI.java:60-68
void SpringAI::checkForHeal() {
	if (isDead() || !getPosition()->isSpawned())
		return;
	bool targetNearby = false; // Java: getKnownList().stream().anyMatch(...)
	for (const runtime::Ptr<world::knownlist::KnownObject>& o : getKnownList().stream()) {
		runtime::Ptr<Creature> creature = runtime::as<Creature>(o->get());
		if (creature != nullptr && getRace() == creature->getRace() && !creature->isDead() && !creature->getLifeStats()->isFullyRestoredHp()
			&& isInRange(*creature, 10) && !creature->getEffectController()->hasAbnormalEffect(19116)) {
			targetNearby = true;
			break;
		}
	}
	if (targetNearby)
		doHeal();
}

// Java SpringAI.java:70-73
void SpringAI::doHeal() {
	AIActions::targetSelf(*this);
	AIActions::useSkill(*this, 19116);
}

} // namespace aion::gameserver::handlers::ai::siege
