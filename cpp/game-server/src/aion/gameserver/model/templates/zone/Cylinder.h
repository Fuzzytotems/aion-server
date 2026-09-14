#pragma once

#include "aion/gameserver/model/templates/zone/Cylinder.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.Cylinder. @author MrPoke */
class Cylinder : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/Cylinder.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::zone
