#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/dataholders/WalkerVersionsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.WalkerVersionsData.
 * <p>
 * C++: afterUnmarshal copies the route version ids and their parent route ids into the @XmlTransient map and clears the bound list, like Java.
 * `routeId` of getRouteVersionId is a plain std::string_view although Java checks it for null: the only caller (WalkerTemplate::getVersionId)
 * passes the required route_id attribute (header request templates-b-3). isRouteVersioned and size come with the P4-09 port.
 */
class WalkerVersionsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WalkerVersionsData.xml.inc"
private:
	std::map<std::string, std::string, std::less<>> walkParents;

public:
	/** @return the id of the route group of a versioned route, nullopt (Java null) otherwise */
	std::optional<std::string> getRouteVersionId(std::string_view routeId) const;
};

} // namespace aion::gameserver::dataholders
