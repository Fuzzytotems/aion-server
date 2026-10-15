#include "aion/gameserver/handlers/ai/instance/raksang/SealGeneratorAI.h"

#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(SealGeneratorAI, "seal_generator");

// Java SealGeneratorAI.java:28-31
bool SealGeneratorAI::canThink() {
	return false;
}

// Java SealGeneratorAI.java:33-39
void SealGeneratorAI::handleCreatureMoved(Creature& creature) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature))) {
		if (PositionUtil::getDistance(getOwner(), *player) <= 30 && startedEvent.compareAndSet(false, true))
			PacketSendUtility::broadcastToMap(getOwner(), 1401156);
	}
}

// Java SealGeneratorAI.java:41-47
bool SealGeneratorAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java SealGeneratorAI.java:49-52
float SealGeneratorAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 1;
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
