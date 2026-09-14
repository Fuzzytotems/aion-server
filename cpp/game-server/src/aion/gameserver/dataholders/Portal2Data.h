#pragma once

#include "aion/gameserver/dataholders/Portal2Data.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.Portal2Data. @author xTz */
class Portal2Data : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/Portal2Data.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
