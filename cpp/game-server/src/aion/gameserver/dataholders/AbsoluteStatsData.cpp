#include "aion/gameserver/dataholders/AbsoluteStatsData.h"

namespace aion::gameserver::dataholders {

void AbsoluteStatsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::stats::AbsoluteStatsTemplate& stats : absoluteStats)
		absoluteStatsData.insert_or_assign(stats.getId(), stats.getModifiers());
	// Java: absoluteStats = null (the C++ index points into the storage, which stays)
}

const model::templates::stats::ModifiersTemplate* AbsoluteStatsData::getTemplate(int32_t statSetId) const {
	auto it = absoluteStatsData.find(statSetId);
	return it != absoluteStatsData.end() ? it->second : nullptr;
}

int32_t AbsoluteStatsData::size() const {
	return static_cast<int32_t>(absoluteStatsData.size());
}

} // namespace aion::gameserver::dataholders
