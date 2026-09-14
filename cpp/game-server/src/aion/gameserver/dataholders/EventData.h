#pragma once

#include "aion/gameserver/dataholders/EventData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.EventData. */
class EventData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/EventData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
