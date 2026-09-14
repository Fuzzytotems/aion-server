#pragma once

#include "aion/gameserver/dataholders/InstanceBuffData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.InstanceBuffData. @author xTz */
class InstanceBuffData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceBuffData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
