#pragma once

#include "aion/gameserver/model/templates/windstreams/StreamLocations.xml.h"

namespace aion::gameserver::model::templates::windstreams {

/** Java com.aionemu.gameserver.model.templates.windstreams.StreamLocations. @author LokiReborn */
class StreamLocations : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/windstreams/StreamLocations.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::windstreams
