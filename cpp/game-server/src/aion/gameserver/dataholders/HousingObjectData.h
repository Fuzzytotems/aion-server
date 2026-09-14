#pragma once

#include "aion/gameserver/dataholders/HousingObjectData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.HousingObjectData. @author Rolandas */
class HousingObjectData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HousingObjectData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
