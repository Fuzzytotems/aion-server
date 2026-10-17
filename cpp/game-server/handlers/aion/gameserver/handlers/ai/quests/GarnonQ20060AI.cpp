#include "aion/gameserver/handlers/ai/quests/GarnonQ20060AI.h"

#include "aion/gameserver/controllers/NpcController.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(GarnonQ20060AI, "Q20060");

// Java GarnonQ20060AI.java:22-27
void GarnonQ20060AI::handleDespawned() {
	NpcAI::handleDespawned();
	runtime::FutureRef t = task.get();
	if (t != nullptr && !t->isDone())
		t->cancel(true);
}

// Java GarnonQ20060AI.java:29-33
void GarnonQ20060AI::handleSpawned() {
	NpcAI::handleSpawned();
	despawn();
}

// Java GarnonQ20060AI.java:35-44
void GarnonQ20060AI::despawn() {
	task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(800020, 442.279f, 464.349f, 341.520f, int8_t{20});
		getOwner().getController().delete_();
	}, 60000 * 3));
}

} // namespace aion::gameserver::handlers::ai::quests
