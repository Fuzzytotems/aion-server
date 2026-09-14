#pragma once

#include "aion/gameserver/model/templates/world/WeatherTable.xml.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WeatherTable. @author Rolandas */
class WeatherTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherTable.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::world
