#pragma once

#include "aion/gameserver/dataholders/TeleporterData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TeleporterData. @author orz */
class TeleporterData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TeleporterData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
