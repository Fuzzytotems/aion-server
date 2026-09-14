#pragma once

#include "aion/gameserver/dataholders/LegionDominionData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.LegionDominionData. @author Yeats */
class LegionDominionData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/LegionDominionData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
