#include "aion/gameserver/handlers/ai/instance/azoturanFortress/BetrayerIcaronixAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::azoturanFortress {

AION_AI(BetrayerIcaronixAI, "betrayericaronix");

// Java BetrayerIcaronixAI.java:24-28
void BetrayerIcaronixAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java BetrayerIcaronixAI.java:30-34
void BetrayerIcaronixAI::handleHpPhase(int32_t /*phaseHpPercent*/) {
	runtime::Ptr<Npc> icaronixTheBetrayer = runtime::cast<Npc>(
		spawn(214599, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), getPosition()->getHeading()));
	icaronixTheBetrayer->getLifeStats()->setCurrentHpPercent(50);
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::azoturanFortress
