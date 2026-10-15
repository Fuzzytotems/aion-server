#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/model/templates/spawns/panesterra/fwd.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::model::templates::spawns::panesterra {

/**
 * An OwnedPart of its SpawnGroup like SpawnTemplate (runtime-architecture.md §9 "Spawn family"). Java's only constructor takes a spot and does
 * not add the template to the group: SpawnGroup's Ahserion's Flight constructor adds it to its spots.
 *
 * @author Yeats
 */
class AhserionsFlightSpawnTemplate : public SpawnTemplate {
private:
	runtime::Field<int32_t> stage{};
	runtime::Field<services::panesterra::ahserion::PanesterraFaction> faction{};

public:
	/** Java: new AhserionsFlightSpawnTemplate(spawnGroup, spot) (not added to the group) */
	AhserionsFlightSpawnTemplate(SpawnGroup& spawnGroup, const SpawnSpotTemplate* spot);

	~AhserionsFlightSpawnTemplate() override;

	int32_t getStage() const { return stage.get(); }

	services::panesterra::ahserion::PanesterraFaction getFaction() const { return faction.get(); }

	void setStage(int32_t value) { stage.set(value); }

	void setPanesterraTeam(services::panesterra::ahserion::PanesterraFaction value) { faction.set(value); }
};

} // namespace aion::gameserver::model::templates::spawns::panesterra
