#include "aion/gameserver/handlers/ai/instance/rakes/SteelRakeKeyBoxAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(SteelRakeKeyBoxAI, "steel_rake_key_box");

// Java SteelRakeKeyBoxAI.java:20-33 (the anonymous Runnable: a task pinning this)
void SteelRakeKeyBoxAI::handleSpawned() {
	ChestAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getOwner().isSpawned()) {
			AIActions::deleteOwner(*this);
		}
	}, 180000);
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
