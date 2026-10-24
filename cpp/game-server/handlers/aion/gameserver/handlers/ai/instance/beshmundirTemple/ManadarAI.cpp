#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/ManadarAI.h"

#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(ManadarAI, "manadar");

// Java ManadarAI.java:24-28
void ManadarAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java ManadarAI.java:30-33
void ManadarAI::handleHpPhase(int32_t phaseHpPercent) {
	trySpawnTraps();
}

// Java ManadarAI.java:35-39
void ManadarAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java ManadarAI.java:41-48
void ManadarAI::trySpawnTraps() {
	if (getPosition()->isSpawned() && !isDead() && hpPhases->getCurrentPhase() > 0) {
		for (int32_t i = 0; i < 5; i++) {
			rndSpawnInRange(commons::utils::Rnd::nextBoolean() ? 281545 : 281756, 4, 11);
		}
		ThreadPoolManager::getInstance().schedule({this}, [this] { trySpawnTraps(); }, 6000);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
