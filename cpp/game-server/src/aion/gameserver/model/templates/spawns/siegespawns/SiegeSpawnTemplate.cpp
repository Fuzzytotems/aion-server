#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"

#include <utility>

#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"

namespace aion::gameserver::model::templates::spawns::siegespawns {

SiegeSpawnTemplate::SiegeSpawnTemplate(int32_t siegeIdValue, model::siege::SiegeRace siegeRaceValue, model::siege::SiegeModType siegeModTypeValue,
	SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot)
	: SpawnTemplate(spawnGroup, spot), siegeId(siegeIdValue), siegeRace(siegeRaceValue), siegeModType(siegeModTypeValue) {
}

SiegeSpawnTemplate::SiegeSpawnTemplate(int32_t siegeIdValue, model::siege::SiegeRace siegeRaceValue, model::siege::SiegeModType siegeModTypeValue,
	SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk, std::optional<std::string_view> walkerId, int32_t staticId)
	: SpawnTemplate(spawnGroup, x, y, z, heading, randWalk, walkerId, staticId), siegeId(siegeIdValue), siegeRace(siegeRaceValue),
	  siegeModType(siegeModTypeValue) {
}

SiegeSpawnTemplate::~SiegeSpawnTemplate() = default;

runtime::Ref<SiegeSpawnTemplate> SiegeSpawnTemplate::create(int32_t siegeIdValue, model::siege::SiegeRace siegeRaceValue,
	model::siege::SiegeModType siegeModTypeValue, SpawnGroup& spawnGroup, float x, float y, float z, int8_t heading, int32_t randWalk,
	std::optional<std::string_view> walkerId, int32_t staticId) {
	return runtime::Ref<SiegeSpawnTemplate>(static_cast<SiegeSpawnTemplate&>(addTemplate(std::make_unique<SiegeSpawnTemplate>(siegeIdValue,
		siegeRaceValue, siegeModTypeValue, spawnGroup, x, y, z, heading, randWalk, walkerId, staticId))));
}

} // namespace aion::gameserver::model::templates::spawns::siegespawns
