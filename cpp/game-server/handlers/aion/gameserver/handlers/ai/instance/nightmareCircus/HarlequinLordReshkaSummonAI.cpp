#include "aion/gameserver/handlers/ai/instance/nightmareCircus/HarlequinLordReshkaSummonAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(HarlequinLordReshkaSummonAI, "harlequinlordreshkasummon");

// Java HarlequinLordReshkaSummonAI.java:19-25
void HarlequinLordReshkaSummonAI::handleSpawned() {
	runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(233453);
	if (boss != nullptr && !boss->isDead()) {
		runtime::Ptr<Creature> mostHated = boss->getAggroList().getTarget(AggroTarget::MOST_HATED);
		if (mostHated != nullptr) // Java: AggroList.isAware(null) is false
			getAggroList().addHate(*mostHated, 1);
	}
	AggressiveNpcAI::handleSpawned();
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
