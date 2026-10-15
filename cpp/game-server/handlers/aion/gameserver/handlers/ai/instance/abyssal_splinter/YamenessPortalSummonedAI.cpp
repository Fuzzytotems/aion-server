#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/YamenessPortalSummonedAI.h"


namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(YamenessPortalSummonedAI, "yamenessportal");

// Java YamenessPortalSummonedAI.java:20-24
void YamenessPortalSummonedAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawnSummons(); }, 12000);
}

// Java YamenessPortalSummonedAI.java:26-37
void YamenessPortalSummonedAI::spawnSummons() {
	if (isDead() || !getOwner().isSpawned()) // ensure npc is still alive and instance is not destroyed yet
		return;
	spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), int8_t{0});
	spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), int8_t{0});
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getOwner().isSpawned()) {
			spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), int8_t{0});
			spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), int8_t{0});
		}
	}, 60000);
}

// Java YamenessPortalSummonedAI.java:39-45
bool YamenessPortalSummonedAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
