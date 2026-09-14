#pragma once

#include "aion/gameserver/dataholders/RoadData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.RoadData. @author SheppeR */
class RoadData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RoadData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
