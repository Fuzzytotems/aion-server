#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionAggressiveNpcAI, "eternal_bastion_aggressive");

// Java EternalBastionAggressiveNpcAI.java:20-26
bool EternalBastionAggressiveNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java EternalBastionAggressiveNpcAI.java:28-33
void EternalBastionAggressiveNpcAI::handleCreatureAggro(Creature& creature) {
	runtime::Ptr<Npc> n = runtime::as<Npc>(runtime::Ptr<Creature>(creature));
	if (n != nullptr && n->getNpcId() == 284685) // Work-around to not aggro the shatter mine
		return;
	AggressiveNpcAI::handleCreatureAggro(creature);
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
