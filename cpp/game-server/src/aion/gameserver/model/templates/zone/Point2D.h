#pragma once

#include "aion/gameserver/model/templates/zone/Point2D.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.Point2D. @author ATracer */
class Point2D : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/Point2D.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::zone
