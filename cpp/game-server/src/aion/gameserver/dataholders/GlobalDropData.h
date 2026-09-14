#pragma once

#include "aion/gameserver/dataholders/GlobalDropData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.GlobalDropData. @author AionCool, Bobobear, Neon */
class GlobalDropData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GlobalDropData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
