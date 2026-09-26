#include "aion/gameserver/dataholders/WalkerVersionsData.h"

namespace aion::gameserver::dataholders {

void WalkerVersionsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::walker::RouteParent& group : routeGroups) {
		for (const model::templates::walker::RouteVersion& version : group.getRouteVersion())
			walkParents.insert_or_assign(version.getId(), group.getId());
	}
	routeGroups.clear(); // Java: routeGroups.clear(); routeGroups = null
}

bool WalkerVersionsData::isRouteVersioned(std::optional<std::string_view> routeId) const {
	if (!routeId)
		return false;
	return walkParents.contains(*routeId);
}

std::optional<std::string> WalkerVersionsData::getRouteVersionId(std::string_view routeId) const {
	auto it = walkParents.find(routeId);
	return it != walkParents.end() ? std::optional<std::string>(it->second) : std::nullopt;
}

int32_t WalkerVersionsData::size() const {
	return static_cast<int32_t>(walkParents.size());
}

} // namespace aion::gameserver::dataholders
