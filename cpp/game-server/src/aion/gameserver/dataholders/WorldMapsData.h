#pragma once

#include "aion/gameserver/dataholders/WorldMapsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WorldMapsData. @author Luno */
class WorldMapsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WorldMapsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
