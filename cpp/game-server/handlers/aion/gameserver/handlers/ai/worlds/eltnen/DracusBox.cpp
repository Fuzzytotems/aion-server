#include "aion/gameserver/handlers/ai/worlds/eltnen/DracusBox.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"

namespace aion::gameserver::handlers::ai::worlds::eltnen {

AION_AI(DracusBox, "dracusbox");

// Java DracusBox.java:26-42. The DeathObserver's lambda pins this AI (and so the deleted crate) until Chaos Dracus dies or despawns.
void DracusBox::handleDied() {
	int32_t spawnId;
	switch (commons::utils::Rnd::get(1, 3)) {
		case 1:
			spawnId = 211792; // elroco
			break;
		case 2:
			spawnId = 211799; // oozing clodworm
			break;
		default:
			spawnId = DRACUS_ID; // chaos dracus
			break;
	}

	Npc& mysteriousCrate = getOwner();
	runtime::Ptr<Npc> spawn =
		runtime::cast<Npc>(NpcAI::spawn(spawnId, mysteriousCrate.getX(), mysteriousCrate.getY(), mysteriousCrate.getZ(), mysteriousCrate.getHeading()));
	AIActions::deleteOwner(*this); // delete the huge box instantly so we can see the spawned mob
	if (spawn->getNpcId() == DRACUS_ID) {
		spawn->getObserveController()->attach(
			*DeathObserver::create(runtime::PinnedCallback<void(Creature&)>({this}, [this](Creature&) { AIActions::scheduleRespawn(*this); })));
	} else {
		AIActions::scheduleRespawn(*this);
	}
}

// Java DracusBox.java:44-50
bool DracusBox::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
			return false;
		default:
			return OneDmgNoActionAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::eltnen
