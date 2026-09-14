#pragma once

#include "aion/gameserver/dataholders/AbsoluteStatsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AbsoluteStatsData. @author Rolandas */
class AbsoluteStatsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AbsoluteStatsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
