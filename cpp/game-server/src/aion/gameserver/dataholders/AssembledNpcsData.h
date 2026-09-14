#pragma once

#include "aion/gameserver/dataholders/AssembledNpcsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AssembledNpcsData. @author xTz */
class AssembledNpcsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AssembledNpcsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
