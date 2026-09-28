#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/TownSpawnsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TownSpawnsData.
 * <p>
 * C++: the index points into the bound `spawnMap` storage, which stays after afterUnmarshal (static-data.md §2.6). The searches walk the maps
 * in Java's HashMap<Integer, TownSpawnMap> iteration order.
 *
 * @author ViAl
 */
class TownSpawnsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TownSpawnsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::towns::TownSpawnMap*> spawnMapsData;
	/** C++ only: spawnMapsData.values() in Java's HashMap iteration order */
	std::vector<const model::templates::towns::TownSpawnMap*> spawnMapsInHashOrder;

public:
	int32_t getSpawnsCount() const;

	/**
	 * @return the spawns of the town level, nullptr (Java null) if no map has the town
	 * @throws NullPointerException if the town has no spawns for the level (Java dereferences the null level)
	 */
	const std::vector<std::unique_ptr<model::templates::spawns::Spawn>>* getSpawns(int32_t townId, int32_t townLevel) const;

	int32_t getWorldIdForTown(int32_t townId) const;

	void addAllNpcIdsToSet(std::unordered_set<int32_t>& npcIds) const;
};

} // namespace aion::gameserver::dataholders
