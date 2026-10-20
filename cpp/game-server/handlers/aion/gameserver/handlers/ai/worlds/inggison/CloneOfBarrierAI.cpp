#include "aion/gameserver/handlers/ai/worlds/inggison/CloneOfBarrierAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

AION_AI(CloneOfBarrierAI, "omegaclone");

// Java CloneOfBarrierAI.java:21-31
void CloneOfBarrierAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	// delay for spawn animation and because KnownList isn't initialized yet
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (runtime::Ptr<Npc> omega = runtime::as<Npc>(getKnownList().getObject(getCreatorId()))) { // Java: instanceof Npc omega
			getOwner().setTarget(omega);
			AIActions::useSkill(*this, 18671);
		}
	}, 3000);
}

// Java CloneOfBarrierAI.java:33-38
void CloneOfBarrierAI::handleDied() {
	AggressiveNpcAI::handleDied();
	if (runtime::Ptr<Npc> omega = runtime::as<Npc>(getKnownList().getObject(getCreatorId()))) // Java: instanceof Npc omega
		omega->getEffectController()->removeEffect(18671);
}

} // namespace aion::gameserver::handlers::ai::worlds::inggison
