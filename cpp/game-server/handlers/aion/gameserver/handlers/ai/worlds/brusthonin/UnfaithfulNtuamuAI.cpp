#include "aion/gameserver/handlers/ai/worlds/brusthonin/UnfaithfulNtuamuAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

namespace aion::gameserver::handlers::ai::worlds::brusthonin {

AION_AI(UnfaithfulNtuamuAI, "unfaithfulntuamu");

// Java UnfaithfulNtuamuAI.java:24-28
void UnfaithfulNtuamuAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java UnfaithfulNtuamuAI.java:30-37. The DeathObserver's lambda pins this AI (and so Ntuamu) until the queen dies or despawns.
void UnfaithfulNtuamuAI::handleHpPhase(int32_t phaseHpPercent) {
	Npc& ntuamu = getOwner();
	runtime::Ptr<Npc> vampireQueen = runtime::cast<Npc>(spawn(214583, ntuamu.getX(), ntuamu.getY(), ntuamu.getZ(), ntuamu.getHeading()));
	vampireQueen->getLifeStats()->setCurrentHpPercent(phaseHpPercent);
	vampireQueen->getObserveController()->attach(
		*DeathObserver::create(runtime::PinnedCallback<void(Creature&)>({this}, [this](Creature&) { AIActions::scheduleRespawn(*this); })));
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::worlds::brusthonin
