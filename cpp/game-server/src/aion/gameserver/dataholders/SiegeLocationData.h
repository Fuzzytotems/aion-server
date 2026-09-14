#pragma once

#include "aion/gameserver/dataholders/SiegeLocationData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SiegeLocationData. @author Sarynth, antness */
class SiegeLocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SiegeLocationData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
