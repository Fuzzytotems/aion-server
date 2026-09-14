#pragma once

#include "aion/gameserver/dataholders/WindstreamData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WindstreamData. @author LokiReborn */
class WindstreamData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WindstreamData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
