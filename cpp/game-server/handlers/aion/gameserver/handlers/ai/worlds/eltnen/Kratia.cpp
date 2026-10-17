#include "aion/gameserver/handlers/ai/worlds/eltnen/Kratia.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"

namespace aion::gameserver::handlers::ai::worlds::eltnen {

AION_AI(Kratia, "kratia");

// Java Kratia.java:23-29. The DeathObserver's lambda pins this AI (and so Kratia) until Harpback dies or despawns.
void Kratia::handleDied() {
	Npc& kratia = getOwner();
	runtime::Ptr<Npc> harpback = runtime::cast<Npc>(spawn(211812, kratia.getX(), kratia.getY(), kratia.getZ(), kratia.getHeading()));
	harpback->getObserveController()->attach(
		*DeathObserver::create(runtime::PinnedCallback<void(Creature&)>({this}, [this](Creature&) { AIActions::scheduleRespawn(*this); })));
	AggressiveNpcAI::handleDied();
}

// Java Kratia.java:31-37
bool Kratia::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::eltnen
