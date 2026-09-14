#pragma once

#include "aion/gameserver/dataholders/FlyRingData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.FlyRingData. @author M@xx */
class FlyRingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/FlyRingData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
