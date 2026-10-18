#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/TiamatsIncarnationAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(TiamatsIncarnationAI, "tiamats_incarnation");

// Java TiamatsIncarnationAI.java:27-31
void TiamatsIncarnationAI::handleActivate() {
	AggressiveNpcAI::handleActivate();
	scheduleSummons(20000);
}

// Java TiamatsIncarnationAI.java:33-47
void TiamatsIncarnationAI::scheduleSummons(int32_t delay) {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getTarget() != nullptr) {
			std::vector<runtime::Ptr<Player>> nearbyPlayers = getNearbyPlayers();
			if (nearbyPlayers.size() > 1) {
				size_t index = static_cast<size_t>(commons::utils::Rnd::nextInt(static_cast<int32_t>(nearbyPlayers.size())));
				runtime::Ptr<Player> first = nearbyPlayers[index];
				nearbyPlayers.erase(nearbyPlayers.begin() + static_cast<std::ptrdiff_t>(index));
				index = static_cast<size_t>(commons::utils::Rnd::nextInt(static_cast<int32_t>(nearbyPlayers.size())));
				runtime::Ptr<Player> second = nearbyPlayers[index];
				nearbyPlayers.erase(nearbyPlayers.begin() + static_cast<std::ptrdiff_t>(index));
				std::vector<int32_t> ids = getSummonNpcIds();
				int32_t summonId = commons::utils::Rnd::get(ids);
				spawn(summonId, first->getX(), first->getY(), first->getZ(), int8_t{0});
				spawn(summonId, second->getX(), second->getY(), second->getZ(), int8_t{0});
				scheduleSummons(30000);
			}
		}
	}, delay);
}

// Java TiamatsIncarnationAI.java:49-51
std::vector<runtime::Ptr<Player>> TiamatsIncarnationAI::getNearbyPlayers() {
	std::vector<runtime::Ptr<Player>> players;
	for (const runtime::Ptr<Player>& player : getKnownList().streamPlayers())
		if (!player->isDead() && isInRange(*player, 30))
			players.push_back(player);
	return players;
}

// Java TiamatsIncarnationAI.java:53-57
void TiamatsIncarnationAI::handleDespawned() {
	for (int32_t id : getSummonNpcIds())
		for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({id}))
			npc->getController().delete_();
	AggressiveNpcAI::handleDespawned();
}

// Java TiamatsIncarnationAI.java:59-76
std::vector<int32_t> TiamatsIncarnationAI::getSummonNpcIds() {
	switch (getNpcId()) {
		case 219366: // Graviwing
			return {282727, 282729}; // Gravity Whirlpool, Thunderbolt Whirlpool
		case 236279: // Graviwing HM
			return {856074, 856076};
		case 219368: // Petriscale
			return {282731}; // Petrification Crystal
		case 236281: // Petriscale HM
			return {856072};
		case 219365: // Fissure Fang
			return {282735, 282737}; // Cavity of Earth, Collapsing Earth
		case 236278: // Fissure Fang HM
			return {856068, 856070};
		default:
			return {};
	}
}

// Java TiamatsIncarnationAI.java:78-84
bool TiamatsIncarnationAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::ALLOW_DECAY:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
