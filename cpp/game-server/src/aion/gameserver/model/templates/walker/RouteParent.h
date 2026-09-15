#pragma once

#include <vector>

#include "aion/gameserver/model/templates/walker/RouteParent.xml.h"

namespace aion::gameserver::model::templates::walker {

/** Java com.aionemu.gameserver.model.templates.walker.RouteParent. */
class RouteParent : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/RouteParent.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<RouteVersion>& getRouteVersion() const { return versions; }
};

} // namespace aion::gameserver::model::templates::walker
