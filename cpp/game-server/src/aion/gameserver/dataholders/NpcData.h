#pragma once

#include "aion/gameserver/dataholders/NpcData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.NpcData. @author Luno */
class NpcData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
