#pragma once

#include "aion/gameserver/model/templates/zone/Points.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.Points. @author ATracer */
class Points : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/Points.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::zone
