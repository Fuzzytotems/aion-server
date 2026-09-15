#include "aion/gameserver/dataholders/RideData.h"

namespace aion::gameserver::dataholders {

void RideData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::ride::RideInfo& info : rides)
		rideInfos.insert_or_assign(info.getNpcId(), &info);
	// Java: rides = null (the C++ index points into the storage, which stays)
}

const model::templates::ride::RideInfo* RideData::getRideInfo(int32_t npcId) const {
	auto it = rideInfos.find(npcId);
	return it != rideInfos.end() ? it->second : nullptr;
}

int32_t RideData::size() const {
	return static_cast<int32_t>(rideInfos.size());
}

} // namespace aion::gameserver::dataholders
