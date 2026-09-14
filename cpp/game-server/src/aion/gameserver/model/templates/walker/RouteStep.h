#pragma once

#include "aion/gameserver/model/templates/walker/RouteStep.xml.h"

namespace aion::gameserver::model::templates::walker {

/** Java com.aionemu.gameserver.model.templates.walker.RouteStep. @author KKnD, Rolandas */
class RouteStep : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/RouteStep.xml.inc"
protected:
	RouteStep() = default; // Java: protected RouteStep()
};

} // namespace aion::gameserver::model::templates::walker
