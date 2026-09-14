#pragma once

#include "aion/gameserver/dataholders/GlobalNpcExclusionData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.GlobalNpcExclusionData. @author bobobear */
class GlobalNpcExclusionData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
