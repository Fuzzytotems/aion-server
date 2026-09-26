#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/model/templates/spawns/SpawnMap.xml.h"

namespace aion::gameserver::model::templates::spawns {

/**
 * Java com.aionemu.gameserver.model.templates.spawns.SpawnMap.
 * <p>
 * C++: the getters return the bound lists, which are empty where Java returns Collections.emptyList(). `getSpawns()` has a non-const overload for
 * SpawnsData, which adds custom spawns to the map it creates with `SpawnMap(mapId)` (Java: `new ArrayList<>()`).
 *
 * @author xTz
 */
class SpawnMap : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/SpawnMap.xml.inc"
public:
	SpawnMap() = default;

	explicit SpawnMap(int32_t mapId);

	const std::vector<std::unique_ptr<Spawn>>& getSpawns() const { return spawns; }

	/** C++ only: the live list (SpawnsData custom spawns) */
	std::vector<std::unique_ptr<Spawn>>& getSpawns() { return spawns; }

	const std::vector<basespawns::BaseSpawn>& getBaseSpawns() const { return baseSpawns; }

	const std::vector<mercenaries::MercenarySpawn>& getMercenarySpawns() const { return mercenarySpawns; }

	const std::vector<riftspawns::RiftSpawn>& getRiftSpawns() const { return riftSpawns; }

	const std::vector<siegespawns::SiegeSpawn>& getSiegeSpawns() const { return siegeSpawns; }

	const std::vector<vortexspawns::VortexSpawn>& getVortexSpawns() const { return vortexSpawns; }

	const std::vector<panesterra::AhserionsFlightSpawn>& getAhserionSpawns() const { return ahserionSpawns; }
};

} // namespace aion::gameserver::model::templates::spawns
