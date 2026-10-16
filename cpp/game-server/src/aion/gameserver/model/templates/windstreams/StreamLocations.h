#pragma once

#include <vector>

#include "aion/gameserver/model/templates/windstreams/StreamLocations.xml.h"

namespace aion::gameserver::model::templates::windstreams {

/** Java com.aionemu.gameserver.model.templates.windstreams.StreamLocations. @author LokiReborn */
class StreamLocations : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/windstreams/StreamLocations.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<Location2D>& getLocation() const { return location; }
};

} // namespace aion::gameserver::model::templates::windstreams
