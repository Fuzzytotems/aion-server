#pragma once

#include "aion/gameserver/dataholders/ChestData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ChestData. @author Wakizashi */
class ChestData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ChestData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
