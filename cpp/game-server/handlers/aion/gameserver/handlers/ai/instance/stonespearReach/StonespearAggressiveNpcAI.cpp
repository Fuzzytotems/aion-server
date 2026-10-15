#include "aion/gameserver/handlers/ai/instance/stonespearReach/StonespearAggressiveNpcAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

AION_AI(StonespearAggressiveNpcAI, "aggressive_stonespear");

// Java StonespearAggressiveNpcAI.java:28-32
void StonespearAggressiveNpcAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	findGuardianStone();
}

// Java StonespearAggressiveNpcAI.java:34-48. java-bug (ported as is, a proposed correction in deviations/P6-I2.md): the list has six of the
// eight guardian stones - the Elyos stage-3 stone 855809 and the Asmodian stage-4 stone 856469 are missing, so those stages' attackers find
// no stone (StonespearReachInstance.spawnGuardianStone)
void StonespearAggressiveNpcAI::findGuardianStone() {
	for (int32_t id : {855763, 855832, 855786, 856466, 856467, 856468})
		guardIds.add(id); // Java: Collections.addAll(guardIds, ...)
	runtime::Ptr<Creature> target = nullptr;
	for (int32_t npcId : guardIds.snapshot()) {
		target = getOwner().getPosition()->getWorldMapInstance()->getNpc(npcId);
		if (target != nullptr) {
			break;
		}
	}
	if (target != nullptr) {
		getOwner().getAggroList().addHate(*target, 3000);
		setStateIfNot(AIState::FIGHT);
		think();
	}
}

// Java StonespearAggressiveNpcAI.java:50-54
void StonespearAggressiveNpcAI::handleDied() {
	AggressiveNoLootNpcAI::handleDied();
	getOwner().getController().delete_();
}

// Java StonespearAggressiveNpcAI.java:56-62
bool StonespearAggressiveNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return AggressiveNoLootNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
