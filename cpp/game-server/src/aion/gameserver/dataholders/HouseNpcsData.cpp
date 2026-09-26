#include "aion/gameserver/dataholders/HouseNpcsData.h"

#include <set>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

using model::templates::spawns::HouseSpawn;
using model::templates::spawns::HouseSpawns;

void HouseNpcsData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	for (const HouseSpawns& houseSpawns : houseSpawnsData) {
		std::set<model::templates::spawns::SpawnType> spawnTypes;
		houseSpawnsByAddressId.insert_or_assign(houseSpawns.getAddress(), &houseSpawns.getSpawns());
		for (const HouseSpawn& spawn : houseSpawns.getSpawns()) {
			if (!spawnTypes.insert(spawn.getType()).second)
				ctx.fail("Duplicate " + std::string(xml::enumName(spawn.getType())) + " spawn for house " + std::to_string(houseSpawns.getAddress()));
		}
	}
	// Java: houseSpawnsData = null (the C++ index points into the storage, which stays)
}

const std::vector<HouseSpawn>* HouseNpcsData::getSpawnsByAddress(int32_t address) const {
	auto it = houseSpawnsByAddressId.find(address);
	return it != houseSpawnsByAddressId.end() ? it->second : nullptr;
}

int32_t HouseNpcsData::size() const {
	int32_t sum = 0;
	for (const auto& [address, spawns] : houseSpawnsByAddressId)
		sum += static_cast<int32_t>(spawns->size());
	return sum;
}

} // namespace aion::gameserver::dataholders
