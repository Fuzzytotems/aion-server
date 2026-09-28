#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/housing/fwd.h"

namespace aion::gameserver::model::templates::spawns::housing {

/**
 * An OwnedPart of its SpawnGroup like SpawnTemplate (runtime-architecture.md §9 "Spawn family"). Java's constructor takes a spot and does not
 * add the template to the group: Town.java:138 hands it to SpawnGroup::adoptDetachedTemplate.
 */
class TownSpawnTemplate : public SpawnTemplate {
private:
	const int32_t townId;

public:
	/** Java: new TownSpawnTemplate(spawnGroup, spot, townId) (not added to the group) */
	TownSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot, int32_t townId);

	~TownSpawnTemplate() override;

	int32_t getTownId() const { return townId; }
};

} // namespace aion::gameserver::model::templates::spawns::housing
