#include "aion/gameserver/handlers/ai/instance/drakenspire/SealBreakerAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(SealBreakerAI, "drakenspire_seal_breaker");

// Java SealBreakerAI.java:20-24
void SealBreakerAI::handleDied() {
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501334); // Arrrgh!
	AggressiveNoLootNpcAI::handleDied();
}

// Java SealBreakerAI.java:26-31
bool SealBreakerAI::ask(AIQuestion question) {
	if (question == AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES)
		return true;
	return AggressiveNoLootNpcAI::ask(question);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
