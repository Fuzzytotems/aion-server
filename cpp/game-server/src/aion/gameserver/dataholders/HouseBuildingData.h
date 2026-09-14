#pragma once

#include "aion/gameserver/dataholders/HouseBuildingData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.HouseBuildingData. @author Rolandas */
class HouseBuildingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HouseBuildingData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
