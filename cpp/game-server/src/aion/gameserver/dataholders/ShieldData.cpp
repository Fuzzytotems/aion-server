#include "aion/gameserver/dataholders/ShieldData.h"

namespace aion::gameserver::dataholders {

int32_t ShieldData::size() const {
	return static_cast<int32_t>(shieldTemplates.size());
}

const std::vector<model::templates::shield::ShieldTemplate>& ShieldData::getShieldTemplates() const {
	return shieldTemplates;
}

} // namespace aion::gameserver::dataholders
