#pragma once

#include "aion/gameserver/dataholders/GatherableData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.GatherableData. @author ATracer */
class GatherableData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GatherableData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
