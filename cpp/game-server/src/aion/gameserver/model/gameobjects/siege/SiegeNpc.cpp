#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"

#include <utility>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"

namespace aion::gameserver::model::gameobjects::siege {

SiegeNpc::SiegeNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller,
	templates::spawns::siegespawns::SiegeSpawnTemplate& spawnTemplate, const templates::npc::NpcTemplate* objectTemplate)
	: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {
}

SiegeNpc::~SiegeNpc() = default;

model::siege::SiegeRace SiegeNpc::getSiegeRace() {
	return getSpawn()->getSiegeRace();
}

int32_t SiegeNpc::getSiegeId() {
	return getSpawn()->getSiegeId();
}

runtime::Ptr<templates::spawns::siegespawns::SiegeSpawnTemplate> SiegeNpc::getSpawn() const {
	return runtime::cast<templates::spawns::siegespawns::SiegeSpawnTemplate>(Npc::getSpawn());
}

bool SiegeNpc::isEnemyFrom(Creature& creature) {
	if (SiegeNpc* siegeNpc = dynamic_cast<SiegeNpc*>(&creature); siegeNpc != nullptr && siegeNpc->getSiegeRace() != getSiegeRace())
		return true;
	return Npc::isEnemyFrom(creature);
}

} // namespace aion::gameserver::model::gameobjects::siege
