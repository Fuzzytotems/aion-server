#include "aion/gameserver/dataholders/HotspotData.h"

namespace aion::gameserver::dataholders {

int32_t HotspotData::size() const {
	return static_cast<int32_t>(hotspotTemplates.size());
}

const std::vector<model::templates::hotspot::HotspotTemplate>& HotspotData::getHotspotTemplates() const {
	return hotspotTemplates;
}

const model::templates::hotspot::HotspotTemplate* HotspotData::getHotspotTemplateById(int32_t id) const {
	for (const model::templates::hotspot::HotspotTemplate& t : hotspotTemplates) {
		if (t.getId() == id)
			return &t;
	}
	return nullptr;
}

} // namespace aion::gameserver::dataholders
