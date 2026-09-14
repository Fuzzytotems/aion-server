#pragma once

#include "aion/gameserver/dataholders/AIData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AIData. @author xTz */
class AIData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AIData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
