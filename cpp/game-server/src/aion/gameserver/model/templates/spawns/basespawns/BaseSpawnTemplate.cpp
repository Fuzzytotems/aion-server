#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"

namespace aion::gameserver::model::templates::spawns::basespawns {

BaseSpawnTemplate::BaseSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot) : SpawnTemplate(spawnGroup, spot) {
}

BaseSpawnTemplate::~BaseSpawnTemplate() = default;

} // namespace aion::gameserver::model::templates::spawns::basespawns
