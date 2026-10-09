#include "aion/gameserver/handlers/ai/AggressiveBossSummonNpcAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai {

AION_AI(AggressiveBossSummonNpcAI, "aggressive_boss_summon");

// Java AggressiveBossSummonNpcAI.java:20-25
void AggressiveBossSummonNpcAI::handleAttackComplete() {
	AggressiveNpcAI::handleAttackComplete();
	if (!isCreatorStillFighting())
		getOwner().getController().delete_();
}

// Java AggressiveBossSummonNpcAI.java:27-30
void AggressiveBossSummonNpcAI::handleFinishAttack() {
	getOwner().getController().delete_();
}

// Java AggressiveBossSummonNpcAI.java:32-35
bool AggressiveBossSummonNpcAI::isCreatorStillFighting() {
	runtime::Ptr<Creature> creator = runtime::as<Creature>(getKnownList().getObject(getCreatorId())); // Java: the pattern variable
	return creator != nullptr && !creator->isDead() && creator->getAggroList().getTarget(AggroTarget::MOST_HATED) != nullptr;
}

// Java AggressiveBossSummonNpcAI.java:37-41
void AggressiveBossSummonNpcAI::handleDied() {
	AggressiveNpcAI::handleDied();
	getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai
