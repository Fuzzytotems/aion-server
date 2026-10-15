#include "aion/gameserver/model/templates/spawns/riftspawns/RiftSpawnTemplate.h"

namespace aion::gameserver::model::templates::spawns::riftspawns {

RiftSpawnTemplate::RiftSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot) : SpawnTemplate(spawnGroup, spot) {
}

RiftSpawnTemplate::~RiftSpawnTemplate() = default;

} // namespace aion::gameserver::model::templates::spawns::riftspawns
