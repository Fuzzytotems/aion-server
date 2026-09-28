#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/fwd.h"
#include "aion/gameserver/model/vortex/VortexStateType.h"

namespace aion::gameserver::model::templates::spawns::vortexspawns {

/**
 * An OwnedPart of its SpawnGroup like SpawnTemplate (runtime-architecture.md §9 "Spawn family"). Java's only constructor takes a spot and does
 * not add the template to the group: SpawnGroup's vortex constructor adds it to its spots.
 *
 * @author Source
 */
class VortexSpawnTemplate : public SpawnTemplate {
private:
	runtime::Field<int32_t> id{};
	runtime::Field<model::vortex::VortexStateType> stateType{};

public:
	/** Java: new VortexSpawnTemplate(spawnGroup, spot) (not added to the group) */
	VortexSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot);

	~VortexSpawnTemplate() override;

	int32_t getId() const { return id.get(); }

	model::vortex::VortexStateType getStateType() const { return stateType.get(); }

	void setId(int32_t value) { id.set(value); }

	void setStateType(model::vortex::VortexStateType value) { stateType.set(value); }

	bool isInvasion() const { return stateType.get() == model::vortex::VortexStateType::INVASION; }

	bool isPeace() const { return stateType.get() == model::vortex::VortexStateType::PEACE; }
};

} // namespace aion::gameserver::model::templates::spawns::vortexspawns
