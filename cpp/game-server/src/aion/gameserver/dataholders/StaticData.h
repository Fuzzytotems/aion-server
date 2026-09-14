#pragma once

#include "aion/gameserver/dataholders/StaticData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.StaticData. @author Luno, orz, Wakizashi */
class StaticData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/StaticData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
