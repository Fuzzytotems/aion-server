#include "aion/gameserver/handlers/ai/worlds/ForgottenGraveAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(ForgottenGraveAI, "forgottengrave");

// Java ForgottenGraveAI.java:23-26
bool ForgottenGraveAI::canThink() {
	return false;
}

// Java ForgottenGraveAI.java:28-35 (Java's cast (Npc) of the spawns; a failed spawn is its NullPointerException in scheduleAddDespawn)
void ForgottenGraveAI::handleDied() {
	GeneralNpcAI::handleDied();
	if (isSpawned.compareAndSet(false, true)) {
		for (int32_t i = 0; i < 2; i++) { // Java: the two identical statements
			runtime::Ptr<Npc> add = runtime::cast<Npc>(rndSpawnInRange(283906, 1, 2));
			if (add == nullptr)
				throw runtime::NullPointerException("ForgottenGraveAI: the spawn of 283906");
			scheduleAddDespawn(*add);
		}
	}
}

// Java ForgottenGraveAI.java:37-40
void ForgottenGraveAI::scheduleAddDespawn(Npc& npc) {
	runtime::Ref<Npc> add(npc);
	npc.getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({&npc}, [add] {
		add->getController().deleteIfAliveOrCancelRespawn();
	}, 5, runtime::TimeUnit::MINUTES));
}

} // namespace aion::gameserver::handlers::ai::worlds
