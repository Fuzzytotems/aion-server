#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/RideData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.RideData.
 * <p>
 * C++: the index points into the bound `rides` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class RideData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RideData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::ride::RideInfo*> rideInfos;

public:
	/** @return the ride info of the npc, nullptr (Java null) if there is none */
	const model::templates::ride::RideInfo* getRideInfo(int32_t npcId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
