#pragma once

#include "aion/gameserver/dataholders/FlyPathData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.FlyPathData. @author KID */
class FlyPathData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/FlyPathData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
