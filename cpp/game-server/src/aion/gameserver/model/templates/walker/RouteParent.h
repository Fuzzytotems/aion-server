#pragma once

#include "aion/gameserver/model/templates/walker/RouteParent.xml.h"

namespace aion::gameserver::model::templates::walker {

/** Java com.aionemu.gameserver.model.templates.walker.RouteParent. */
class RouteParent : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/RouteParent.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::walker
