#pragma once

#include "aion/gameserver/dataholders/StaticDoorData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.StaticDoorData. @author Wakizashi */
class StaticDoorData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/StaticDoorData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
