#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/StaticDoorData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.StaticDoorData.
 * <p>
 * C++: the index points into the bound `staticDoorWorlds` storage, which stays after afterUnmarshal (static-data.md §2.6). A duplicate world
 * fails the load through LoadContext::fail with Java's IllegalArgumentException message.
 *
 * @author Wakizashi
 */
class StaticDoorData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/StaticDoorData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::staticdoor::StaticDoorWorld*> doorWorlds;

public:
	int32_t size() const;

	/** @return the doors of the world, empty (Java Collections.emptyList()) if there are none */
	const std::vector<const model::templates::staticdoor::StaticDoorTemplate*>& getStaticDoors(int32_t worldId) const;

	/** @return the door, nullptr (Java null) if there is none */
	const model::templates::staticdoor::StaticDoorTemplate* getStaticDoor(int32_t worldId, int32_t staticId) const;
};

} // namespace aion::gameserver::dataholders
