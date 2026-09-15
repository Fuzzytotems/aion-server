#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/fwd.h"

namespace aion::gameserver::model::templates::spawns::riftspawns {

/**
 * An OwnedPart of its SpawnGroup like SpawnTemplate (runtime-architecture.md §9 "Spawn family"). Java's only constructor takes a spot and does
 * not add the template to the group: SpawnGroup's rift constructor adds it to its spots.
 *
 * @author Source
 */
class RiftSpawnTemplate : public SpawnTemplate {
private:
	runtime::Field<int32_t> id{};

public:
	/** Java: new RiftSpawnTemplate(spawnGroup, spot) (not added to the group) */
	RiftSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot);

	~RiftSpawnTemplate() override;

	int32_t getId() const { return id.get(); }

	void setId(int32_t value) { id.set(value); }
};

} // namespace aion::gameserver::model::templates::spawns::riftspawns
