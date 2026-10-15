#include "aion/gameserver/dataholders/WindstreamData.h"

namespace aion::gameserver::dataholders {

void WindstreamData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::windstreams::WindstreamTemplate& wt : wts)
		windstreams.insert_or_assign(wt.getMapId(), &wt);
	// Java: wts = null (the C++ index points into the storage, which stays)
}

const model::templates::windstreams::WindstreamTemplate* WindstreamData::getStreamTemplate(int32_t mapId) const {
	auto it = windstreams.find(mapId);
	return it != windstreams.end() ? it->second : nullptr;
}

int32_t WindstreamData::size() const {
	return static_cast<int32_t>(windstreams.size());
}

} // namespace aion::gameserver::dataholders
