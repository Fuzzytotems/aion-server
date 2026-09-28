#include "aion/gameserver/model/templates/spawns/vortexspawns/VortexSpawnTemplate.h"

namespace aion::gameserver::model::templates::spawns::vortexspawns {

VortexSpawnTemplate::VortexSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot) : SpawnTemplate(spawnGroup, spot) {
}

VortexSpawnTemplate::~VortexSpawnTemplate() = default;

} // namespace aion::gameserver::model::templates::spawns::vortexspawns
