#pragma once

#include "aion/gameserver/model/templates/world/WeatherEntry.xml.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WeatherEntry. @author Rolandas */
class WeatherEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherEntry.xml.inc"
private:
	WeatherEntry() = default; // Java: private WeatherEntry()
};

} // namespace aion::gameserver::model::templates::world
