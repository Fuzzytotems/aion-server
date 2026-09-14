#pragma once

#include "aion/gameserver/dataholders/HouseNpcsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.HouseNpcsData. @author Rolandas */
class HouseNpcsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HouseNpcsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
