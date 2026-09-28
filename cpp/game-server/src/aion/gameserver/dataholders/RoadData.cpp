#include "aion/gameserver/dataholders/RoadData.h"

namespace aion::gameserver::dataholders {

int32_t RoadData::size() const {
	return static_cast<int32_t>(roadTemplates.size());
}

const std::vector<model::templates::road::RoadTemplate>& RoadData::getRoadTemplates() const {
	return roadTemplates;
}

} // namespace aion::gameserver::dataholders
