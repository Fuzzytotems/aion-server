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

// Java StonespearAggressiveNpcAI.java:34-48. Correction of the Java code (owner's decision 2026-10-19, both branches; was J-I2-1): the list
// has all eight guardian stones; Java's missed the Elyos stage-3 stone 855809 and the Asmodian stage-4 stone 856469, so those stages'
// attackers found no stone (StonespearReachInstance.spawnGuardianStone)
void StonespearAggressiveNpcAI::findGuardianStone() {
	for (int32_t id : {855763, 855832, 855786, 855809, 856466, 856467, 856468, 856469}) // the correction (was J-I2-1)
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
