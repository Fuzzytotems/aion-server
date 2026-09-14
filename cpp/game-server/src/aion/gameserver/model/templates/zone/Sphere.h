#pragma once

#include "aion/gameserver/model/templates/zone/Sphere.xml.h"

namespace aion::gameserver::model::templates::zone {

/** Java com.aionemu.gameserver.model.templates.zone.Sphere. @author MrPoke */
class Sphere : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/zone/Sphere.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::zone
