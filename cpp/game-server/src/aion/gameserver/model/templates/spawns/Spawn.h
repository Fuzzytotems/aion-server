#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/spawns/Spawn.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.Spawn. @author xTz, Rolandas */
class Spawn : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/Spawn.xml.inc"
public:
	Spawn() = default;

	/** Java `Spawn(int npcId, int respawnTime, SpawnHandlerType handler)` (SpawnsData custom spawns; the handler may be null) */
	Spawn(int32_t npcId, int32_t respawnTime, std::optional<spawnengine::SpawnHandlerType> handler);

	int32_t getPool() const { return pool; }

	int32_t getRespawnTime() const { return respawnTime; }

	/** Java creates the list on first use; the C++ vector always exists. SpawnsData adds and replaces spots of custom spawns. */
	std::vector<SpawnSpotTemplate>& getSpawnSpotTemplates() { return spawnTemplates; }

	/** C++ only: read access for `const Spawn*` (SpawnGroup constructors) */
	const std::vector<SpawnSpotTemplate>& getSpawnSpotTemplates() const { return spawnTemplates; }

	bool isCustom() const { return isCustom_; }

	void setCustom(bool value) { isCustom_ = value; }

	bool isEventSpawn() const { return eventTemplate.get() != nullptr; }
};

} // namespace aion::gameserver::model::templates::spawns
