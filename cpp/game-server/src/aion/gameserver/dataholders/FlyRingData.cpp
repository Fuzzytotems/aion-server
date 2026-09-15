#include "aion/gameserver/dataholders/FlyRingData.h"

namespace aion::gameserver::dataholders {

int32_t FlyRingData::size() const {
	return static_cast<int32_t>(flyRingTemplates.size());
}

const std::vector<model::templates::flyring::FlyRingTemplate>& FlyRingData::getFlyRingTemplates() const {
	return flyRingTemplates;
}

} // namespace aion::gameserver::dataholders
