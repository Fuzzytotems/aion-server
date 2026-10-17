#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/MonolithicAmbusherAI.h"

#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(MonolithicAmbusherAI, "monolithicambusher");

// Java MonolithicAmbusherAI.java:22-26
void MonolithicAmbusherAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hasHelped = false;
}

// Java MonolithicAmbusherAI.java:28-35
void MonolithicAmbusherAI::handleCreatureAggro(Creature& creature) {
	AggressiveNpcAI::handleCreatureAggro(creature);
	if (!hasHelped.get()) {
		hasHelped = true;
		help(creature);
	}
}

// Java MonolithicAmbusherAI.java:37-45
void MonolithicAmbusherAI::help(Creature& creature) {
	getKnownList().forEachNpc([this, &creature](Npc& npc) {
		if (isInRange(npc, 60)) {
			if (!npc.isDead() && npc.getNpcId() == 216215 &&
				static_cast<int32_t>(npc.getSpawn()->getY()) == static_cast<int32_t>(getSpawnTemplate()->getY())) {
				npc.getAi().onCreatureEvent(gameserver::ai::event::AIEventType::CREATURE_AGGRO, creature);
			}
		}
	});
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
