#pragma once

#include "aion/gameserver/dataholders/TemperingData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TemperingData. @author xTz */
class TemperingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TemperingData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
