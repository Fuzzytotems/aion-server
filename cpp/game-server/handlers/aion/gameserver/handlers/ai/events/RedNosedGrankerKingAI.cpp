#include "aion/gameserver/handlers/ai/events/RedNosedGrankerKingAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(RedNosedGrankerKingAI, "rednosedgrankerking");

// Java RedNosedGrankerKingAI.java:23-27
void RedNosedGrankerKingAI::handleAttack(runtime::Ptr<Creature> creature) {
	OneDmgAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java RedNosedGrankerKingAI.java:29-39
void RedNosedGrankerKingAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (getNpcId()) {
		case 219292:
		case 219294: {
			int32_t npcId = getNpcId() + 1;
			rndSpawnInRange(npcId, 1, 3);
			rndSpawnInRange(npcId, 1, 3);
			rndSpawnInRange(npcId, 1, 3);
			break;
		}
	}
}

// Java RedNosedGrankerKingAI.java:41-45
void RedNosedGrankerKingAI::handleBackHome() {
	OneDmgAI::handleBackHome();
	hpPhases->reset();
}

// Java RedNosedGrankerKingAI.java:47-58
void RedNosedGrankerKingAI::handleDied() {
	int64_t chestSpawnCount = 0; // Java: getAggroList().stream().filter(a -> a.getAttacker() instanceof Player p && isInRange(p, 50)).count()
	for (const runtime::Ptr<controllers::attack::AggroInfo>& a : getAggroList().stream()) {
		runtime::Ptr<Player> p = runtime::as<Player>(a->getAttacker());
		if (p != nullptr && isInRange(*p, 50))
			++chestSpawnCount;
	}
	for (int32_t i = 0; i < chestSpawnCount; i++)
		rndSpawnInRange(701457, 1, 6); // stolen solorius box
	getOwner().getKnownList().forEachNpc([](Npc& n) {
		if (n.getNpcId() == 219293 || n.getNpcId() == 219295)
			n.getController().delete_();
	});
	OneDmgAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::events
