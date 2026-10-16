#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.h"

#include <string>
#include <utility>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/detail/JavaHashMap.h"

namespace aion::gameserver::model::templates::staticdoor {

void StaticDoorWorld::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	templatesByStaticId.clear();
	std::vector<std::pair<int32_t, const StaticDoorTemplate*>> puts;
	puts.reserve(templates.size());
	for (const StaticDoorTemplate& doorTemplate : templates) {
		// Deviation: Java throws IllegalArgumentException; LoadContext::fail throws StaticDataException with the message (P4-07b.md)
		if (!templatesByStaticId.tryEmplace(doorTemplate.getId(), &doorTemplate))
			ctx.fail("Duplicate door template for world " + std::to_string(worldId) + ", id: " + std::to_string(doorTemplate.getId()));
		puts.emplace_back(doorTemplate.getId(), &doorTemplate);
	}
	staticDoorsInHashOrder = ::aion::gameserver::model::templates::detail::javaIntegerHashMapValues(puts);
}

const StaticDoorTemplate* StaticDoorWorld::getStaticDoor(int32_t staticId) const {
	const StaticDoorTemplate* const* door = templatesByStaticId.find(staticId);
	return door == nullptr ? nullptr : *door;
}

} // namespace aion::gameserver::model::templates::staticdoor
