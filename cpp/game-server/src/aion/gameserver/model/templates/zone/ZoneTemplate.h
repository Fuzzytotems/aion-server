#pragma once

#include "aion/gameserver/model/templates/zone/ZoneTemplate.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.ZoneTemplate. @author ATracer */
class ZoneTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/ZoneTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::zone
