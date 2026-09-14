#pragma once

#include "aion/gameserver/dataholders/TownSpawnsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TownSpawnsData. @author ViAl */
class TownSpawnsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TownSpawnsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
