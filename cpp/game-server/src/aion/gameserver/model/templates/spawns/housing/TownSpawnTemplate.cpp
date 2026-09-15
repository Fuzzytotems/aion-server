#include "aion/gameserver/model/templates/spawns/housing/TownSpawnTemplate.h"

namespace aion::gameserver::model::templates::spawns::housing {

TownSpawnTemplate::TownSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot, int32_t townIdValue)
	: SpawnTemplate(spawnGroup, spot), townId(townIdValue) {
}

TownSpawnTemplate::~TownSpawnTemplate() = default;

} // namespace aion::gameserver::model::templates::spawns::housing
