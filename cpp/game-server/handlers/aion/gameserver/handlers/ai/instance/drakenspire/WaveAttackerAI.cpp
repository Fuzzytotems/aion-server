#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveAttackerAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(WaveAttackerAI, "wave_attacker");

// Java WaveAttackerAI.java:20-30
void WaveAttackerAI::handleCreatureDetected(Creature& creature) {
	AggressiveNoLootNpcAI::handleCreatureDetected(creature);
	if (creature.getTribe() == TribeClass::IDSEAL_PCGUARD) {
		for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs({236248}))
			getOwner().getAggroList().addHate(*npc, 10000);

		for (const runtime::Ptr<Npc>& npc : getOwner().getPosition()->getWorldMapInstance()->getNpcs({236249}))
			getOwner().getAggroList().addHate(*npc, 10000);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
