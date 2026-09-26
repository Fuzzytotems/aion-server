#include "aion/gameserver/dataholders/TownSpawnsData.h"

#include <string>

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/towns/TownLevel.h"
#include "aion/gameserver/model/templates/towns/TownSpawn.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::templates::towns::TownLevel;
using model::templates::towns::TownSpawn;
using model::templates::towns::TownSpawnMap;

void TownSpawnsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const TownSpawnMap*> order;
	for (const TownSpawnMap& map : spawnMap) {
		spawnMapsData.insert_or_assign(map.getMapId(), &map);
		order.put(map.getMapId(), &map, detail::javaHashCode(map.getMapId()));
	}
	spawnMapsInHashOrder = order.values();
	// Java: spawnMap = null (the C++ index points into the storage, which stays)
}

int32_t TownSpawnsData::getSpawnsCount() const {
	int32_t counter = 0;
	for (const TownSpawnMap* map : spawnMapsInHashOrder) {
		for (const TownSpawn* townSpawn : map->getTownSpawns()) {
			for (const TownLevel* townLevel : townSpawn->getTownLevels())
				counter += static_cast<int32_t>(townLevel->getSpawns().size());
		}
	}
	return counter;
}

const std::vector<std::unique_ptr<model::templates::spawns::Spawn>>* TownSpawnsData::getSpawns(int32_t townId, int32_t townLevel) const {
	for (const TownSpawnMap* map : spawnMapsInHashOrder) {
		const TownSpawn* townSpawn = map->getTownSpawn(townId);
		if (townSpawn != nullptr) {
			const TownLevel* level = townSpawn->getSpawnsForLevel(townLevel);
			if (level == nullptr)
				throw runtime::NullPointerException("Cannot invoke \"TownLevel.getSpawns()\": town " + std::to_string(townId) + " has no level " +
				                                    std::to_string(townLevel));
			return &level->getSpawns();
		}
	}
	return nullptr;
}

int32_t TownSpawnsData::getWorldIdForTown(int32_t townId) const {
	for (const TownSpawnMap* map : spawnMapsInHashOrder) {
		if (map->getTownSpawn(townId) != nullptr)
			return map->getMapId();
	}
	return 0;
}

void TownSpawnsData::addAllNpcIdsToSet(std::unordered_set<int32_t>& npcIds) const {
	for (const TownSpawnMap* map : spawnMapsInHashOrder) {
		for (const TownSpawn* townSpawn : map->getTownSpawns()) {
			for (const TownLevel* townLevel : townSpawn->getTownLevels()) {
				for (const std::unique_ptr<model::templates::spawns::Spawn>& spawn : townLevel->getSpawns())
					npcIds.insert(spawn->getNpcId());
			}
		}
	}
}

} // namespace aion::gameserver::dataholders
