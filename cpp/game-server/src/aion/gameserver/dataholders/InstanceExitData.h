#pragma once

#include "aion/gameserver/dataholders/InstanceExitData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.InstanceExitData. @author xTz */
class InstanceExitData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceExitData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
