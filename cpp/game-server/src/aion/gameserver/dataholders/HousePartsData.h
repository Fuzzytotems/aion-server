#pragma once

#include "aion/gameserver/dataholders/HousePartsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.HousePartsData. @author Rolandas */
class HousePartsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HousePartsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
