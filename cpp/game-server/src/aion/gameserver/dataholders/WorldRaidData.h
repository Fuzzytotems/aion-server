#pragma once

#include "aion/gameserver/dataholders/WorldRaidData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WorldRaidData. @author Alcapwnd, Whoop, Sykra */
class WorldRaidData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WorldRaidData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
