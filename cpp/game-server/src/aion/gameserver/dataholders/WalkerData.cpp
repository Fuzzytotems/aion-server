#include "aion/gameserver/dataholders/WalkerData.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {

using model::templates::walker::WalkerTemplate;

void WalkerData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.WalkerData");
	for (const WalkerTemplate& route : walkerlist) {
		if (walkerlistData.putIfAbsent(route.getRouteId(), &route))
			log.warn("Duplicate route ID: " + route.getRouteId());
	}
	// Java: walkerlist.clear(); walkerlist = null (the C++ index points into the storage, which stays)
}

int32_t WalkerData::size() const {
	return static_cast<int32_t>(walkerlistData.size());
}

const WalkerTemplate* WalkerData::getWalkerTemplate(std::string_view routeId) const {
	const auto* entry = walkerlistData.get(std::string(routeId));
	return entry != nullptr ? *entry : nullptr;
}

void WalkerData::addTemplate(std::unique_ptr<WalkerTemplate> /*newTemplate*/) {
	AION_UNPORTED(); // write-back of FixPath routes (static-data.md §3.6 item 7)
}

void WalkerData::saveData(std::string_view /*routeId*/) {
	AION_UNPORTED(); // write-back of FixPath routes (static-data.md §3.6 item 7)
}

std::vector<const WalkerTemplate*> WalkerData::getTemplates() const {
	std::vector<const WalkerTemplate*> templates;
	templates.reserve(walkerlistData.size());
	for (const auto& [routeId, route] : walkerlistData)
		templates.push_back(route);
	return templates;
}

} // namespace aion::gameserver::dataholders
