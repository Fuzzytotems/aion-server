#include "aion/gameserver/handlers/ai/instance/darkPoeta/MarabataAI.h"

#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(MarabataAI, "marabata");

// Java MarabataAI.java:26-61
void MarabataAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isStarted.compareAndSet(false, true)) {
		boosterLifeCheckTask = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
			if (isDead())
				return;
			switch (getNpcId()) {
				case 214849:
					if (!isBoosterAlive(700439))
						spawn(700439, 665.374f, 372.751f, 99.375f, int8_t{90});
					if (!isBoosterAlive(700440))
						spawn(700440, 681.851f, 408.625f, 100.472f, int8_t{13});
					if (!isBoosterAlive(700441))
						spawn(700441, 646.550f, 406.088f, 99.375f, int8_t{49});
					break;
				case 214850:
					if (!isBoosterAlive(700442))
						spawn(700442, 636.118f, 325.537f, 99.375f, int8_t{49});
					if (!isBoosterAlive(700443))
						spawn(700443, 676.257f, 319.650f, 99.375f, int8_t{4});
					if (!isBoosterAlive(700444))
						spawn(700444, 655.851f, 292.711f, 99.375f, int8_t{90});
					break;
				case 214851:
					if (!isBoosterAlive(700445))
						spawn(700445, 605.625f, 380.479f, 99.375f, int8_t{14});
					if (!isBoosterAlive(700446))
						spawn(700446, 598.706f, 345.978f, 99.375f, int8_t{98});
					if (!isBoosterAlive(700447))
						spawn(700447, 567.775f, 366.207f, 99.375f, int8_t{59});
					break;
			}
		}, 30000, 30000);
	}
}

// Java MarabataAI.java:63-83
void MarabataAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	switch (getNpcId()) {
		case 214849:
			spawn(700439, 665.374f, 372.751f, 99.375f, int8_t{90});
			spawn(700440, 681.851f, 408.625f, 100.472f, int8_t{13});
			spawn(700441, 646.550f, 406.088f, 99.375f, int8_t{49});
			break;
		case 214850:
			spawn(700442, 636.118f, 325.537f, 99.375f, int8_t{49});
			spawn(700443, 676.257f, 319.650f, 99.375f, int8_t{4});
			spawn(700444, 655.851f, 292.711f, 99.375f, int8_t{90});
			break;
		case 214851:
			spawn(700445, 605.625f, 380.479f, 99.375f, int8_t{14});
			spawn(700446, 598.706f, 345.978f, 99.375f, int8_t{98});
			spawn(700447, 567.775f, 366.207f, 99.375f, int8_t{59});
			break;
	}
}

// Java MarabataAI.java:85-87
bool MarabataAI::isBoosterAlive(int32_t boosterId) {
	return getPosition()->getWorldMapInstance()->getNpc(boosterId) != nullptr;
}

// Java MarabataAI.java:89-92
void MarabataAI::cancelTask() {
	runtime::FutureRef task = boosterLifeCheckTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java MarabataAI.java:94-98
void MarabataAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTask();
}

// Java MarabataAI.java:100-104
void MarabataAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
