#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/HouseNpcsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.HouseNpcsData.
 * <p>
 * C++: the index points into the bound `houseSpawnsData` storage, which stays after afterUnmarshal (static-data.md §2.6). A duplicate spawn
 * type fails the load through LoadContext::fail with Java's IllegalArgumentException message.
 *
 * @author Rolandas
 */
class HouseNpcsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HouseNpcsData.xml.inc"
private:
	std::unordered_map<int32_t, const std::vector<model::templates::spawns::HouseSpawn>*> houseSpawnsByAddressId;

public:
	/** @return the spawns of the house address, nullptr (Java null) if there are none */
	const std::vector<model::templates::spawns::HouseSpawn>* getSpawnsByAddress(int32_t address) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
