#include "aion/gameserver/handlers/ai/instance/nightmareCircus/YumeAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(YumeAI, "yume");

// Java YumeAI.java:31-34
bool YumeAI::canThink() {
	return false;
}

// Java YumeAI.java:36-39
void YumeAI::handleDialogStart(Player& player) {
}

// Java YumeAI.java:41-46
void YumeAI::onEffectApplied(Effect& effect) {
	switch (effect.getSkillId()) {
		case 21463:
		case 21465:
			AIActions::useSkill(*this, 21467);
			break;
		default:
			break;
	}
}

// Java YumeAI.java:48-62 (the task keeps the player alive, as Java's capture does)
void YumeAI::handleCreatureMoved(Creature& creature) {
	if (runtime::Ptr<Player> found = runtime::as<Player>(runtime::Ptr<Creature>(creature))) {
		if (isStart.compareAndSet(false, true)) {
			runtime::Ref<Player> p(*found);
			skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, p] {
				if (p->getLifeStats()->getHpPercentage() < 100) {
					if (commons::utils::Rnd::nextBoolean()) {
						PacketSendUtility::broadcastMessage(getOwner(), 1501126);
					}
					AIActions::useSkill(*this, 21466);
				}
			}, 15000, 15000));
		}
	}
}

// Java YumeAI.java:64-68
void YumeAI::cancelTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java YumeAI.java:70-74
void YumeAI::handleDespawned() {
	cancelTask();
	GeneralNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
