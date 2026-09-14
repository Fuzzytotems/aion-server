#pragma once

#include "aion/gameserver/dataholders/PlayerExperienceTable.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PlayerExperienceTable. @author Luno */
class PlayerExperienceTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerExperienceTable.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
