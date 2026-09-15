#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/HouseBuildingData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.HouseBuildingData.
 * <p>
 * C++: the @XmlTransient index points into the bound `buildings` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null). A duplicate building id fails the load through LoadContext::fail with Java's IllegalArgumentException message. size comes with
 * the P4-09 port (header request templates-b-2 added getBuilding).
 *
 * @author Rolandas
 */
class HouseBuildingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HouseBuildingData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::housing::Building*> buildingById;

public:
	/** @return the building, nullptr (Java null) if there is none */
	const model::templates::housing::Building* getBuilding(int32_t buildingId) const;
};

} // namespace aion::gameserver::dataholders
