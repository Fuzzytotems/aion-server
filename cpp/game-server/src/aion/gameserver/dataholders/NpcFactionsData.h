#pragma once

#include "aion/gameserver/dataholders/NpcFactionsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.NpcFactionsData. @author vlog */
class NpcFactionsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcFactionsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
