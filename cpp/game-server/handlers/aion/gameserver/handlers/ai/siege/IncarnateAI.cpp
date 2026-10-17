#include "aion/gameserver/handlers/ai/siege/IncarnateAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(IncarnateAI, "incarnate");

// Java IncarnateAI.java:21-29 (spawn for quest)
void IncarnateAI::handleDied() {
	SiegeNpcAI::handleDied();
	if (getOwner().getNpcId() == 259614) {
		spawn(701237, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
		despawnClaw();
	}
}

// Java IncarnateAI.java:31-40 (Java's anonymous Runnable; a missing claw is its NullPointerException when the task runs)
void IncarnateAI::despawnClaw() {
	runtime::Ref<Npc> claw(getPosition()->getWorldMapInstance()->getNpc(701237));
	ThreadPoolManager::getInstance().schedule({this}, [claw] {
		if (claw == nullptr)
			throw runtime::NullPointerException("IncarnateAI: claw 701237");
		claw->getController().delete_();
	}, 60000 * 5);
}

} // namespace aion::gameserver::handlers::ai::siege
