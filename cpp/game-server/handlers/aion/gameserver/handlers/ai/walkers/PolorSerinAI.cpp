#include "aion/gameserver/handlers/ai/walkers/PolorSerinAI.h"

#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::ai::walkers {

AION_AI(PolorSerinAI, "polorserin");

// Java PolorSerinAI.java:18-28
void PolorSerinAI::handleMoveArrived() {
	bool adultsNear = false; // Java: getKnownList().stream().anyMatch(...)
	for (const runtime::Ptr<world::knownlist::KnownObject>& obj : getKnownList().stream()) {
		runtime::Ptr<Npc> npc = runtime::as<Npc>(obj->get());
		if (npc != nullptr && (npc->getNpcId() == 203129 || npc->getNpcId() == 203132) && isInRange(*npc, getOwner().getAggroRange())) {
			adultsNear = true;
			break;
		}
	}
	if (adultsNear) {
		gameserver::ai::handler::MoveEventHandler::onMoveArrived(*this);
		getOwner().unsetState(CreatureState::WEAPON_EQUIPPED);
	} else {
		WalkGeneralRunnerAI::handleMoveArrived();
	}
}

} // namespace aion::gameserver::handlers::ai::walkers
