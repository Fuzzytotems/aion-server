#pragma once

#include "aion/gameserver/dataholders/ShieldData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ShieldData. @author Wakizashi */
class ShieldData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ShieldData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
