#pragma once

#include "aion/gameserver/dataholders/ZoneData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ZoneData. @author ATracer */
class ZoneData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ZoneData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
