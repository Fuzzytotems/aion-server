#include "aion/gameserver/handlers/ai/instance/dredgion/SurkanaAI.h"

#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::dredgion {

AION_AI(SurkanaAI, "surkana");

// Java SurkanaAI.java:23-27
void SurkanaAI::handleAttack(runtime::Ptr<Creature> creature) {
	OneDmgNoActionAI::handleAttack(creature);
	// roomaggro
	checkForSupport(*creature);
}

// Java SurkanaAI.java:29-34
void SurkanaAI::checkForSupport(Creature& creature) {
	getKnownList().forEachNpc([this, &creature](Npc& npc) {
		if (!npc.isDead() && isInRange(npc, 25))
			npc.getAi().onCreatureEvent(AIEventType::CREATURE_AGGRO, creature);
	});
}

} // namespace aion::gameserver::handlers::ai::instance::dredgion
