#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/WorldRaidData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.WorldRaidData.
 * <p>
 * C++: the index points into the bound `worldRaidLocations` storage, which stays (Java clears the list). getLocations returns the index as a
 * read-only map that iterates in Java's HashMap order (computed by afterUnmarshal; the //worldraid admin command lists the locations in it).
 *
 * @author Alcapwnd, Whoop, Sykra
 */
class WorldRaidData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WorldRaidData.xml.inc"
private:
	/** in Java's HashMap iteration order */
	detail::LinkedMap<int32_t, const model::templates::worldraid::WorldRaidLocation*> locationsById;

public:
	/** @return the location, nullptr (Java null) if there is none */
	const model::templates::worldraid::WorldRaidLocation* getLocationsById(int32_t locationId) const;

	/** Java: the HashMap; C++: a read-only map that iterates in Java's HashMap order */
	const detail::LinkedMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>& getLocations() const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
