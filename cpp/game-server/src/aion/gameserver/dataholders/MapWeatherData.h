#pragma once

#include "aion/gameserver/dataholders/MapWeatherData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.MapWeatherData. @author Rolandas */
class MapWeatherData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MapWeatherData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
