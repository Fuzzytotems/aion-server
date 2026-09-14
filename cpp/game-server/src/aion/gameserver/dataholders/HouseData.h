#pragma once

#include "aion/gameserver/dataholders/HouseData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.HouseData. @author Rolandas */
class HouseData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HouseData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
