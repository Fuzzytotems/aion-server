#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/basespawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::templates::spawns::basespawns {

/**
 * An OwnedPart of its SpawnGroup like SpawnTemplate (runtime-architecture.md §9 "Spawn family"). Java's only constructor takes a spot and does
 * not add the template to the group: SpawnGroup's base spawn constructor adds it to its spots.
 *
 * @author Source
 */
class BaseSpawnTemplate : public SpawnTemplate {
private:
	runtime::Field<int32_t> id{};
	runtime::Field<model::base::BaseOccupier> occupier{};

public:
	/** Java: new BaseSpawnTemplate(spawnGroup, spot) (not added to the group) */
	BaseSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot);

	~BaseSpawnTemplate() override;

	int32_t getId() const { return id.get(); }

	void setId(int32_t value) { id.set(value); }

	model::base::BaseOccupier getOccupier() const { return occupier.get(); }

	void setOccupier(model::base::BaseOccupier value) { occupier.set(value); }
};

} // namespace aion::gameserver::model::templates::spawns::basespawns
