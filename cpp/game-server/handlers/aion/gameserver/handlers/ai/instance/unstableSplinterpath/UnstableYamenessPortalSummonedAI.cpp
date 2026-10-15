#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableYamenessPortalSummonedAI.h"


namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableYamenessPortalSummonedAI, "unstableyamenessportal");

// Java UnstableYamenessPortalSummonedAI.java:20-24
void UnstableYamenessPortalSummonedAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { spawnSummons(); }, 12000);
}

// Java UnstableYamenessPortalSummonedAI.java:26-37
void UnstableYamenessPortalSummonedAI::spawnSummons() {
	if (isDead() || !getOwner().isSpawned()) // ensure npc is still alive and instance is not destroyed yet
		return;
	spawn(219565, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), int8_t{0});
	spawn(219566, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), int8_t{0});
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getOwner().isSpawned()) {
			spawn(219565, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), int8_t{0});
			spawn(219566, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), int8_t{0});
		}
	}, 60000);
}

// Java UnstableYamenessPortalSummonedAI.java:39-45
bool UnstableYamenessPortalSummonedAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
