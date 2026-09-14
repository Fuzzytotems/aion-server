#pragma once

#include "aion/gameserver/dataholders/AssemblyItemsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AssemblyItemsData. @author xTz */
class AssemblyItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AssemblyItemsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
