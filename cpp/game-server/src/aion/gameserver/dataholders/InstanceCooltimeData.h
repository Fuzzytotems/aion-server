#pragma once

#include "aion/gameserver/dataholders/InstanceCooltimeData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.InstanceCooltimeData. @author VladimirZ */
class InstanceCooltimeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceCooltimeData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
