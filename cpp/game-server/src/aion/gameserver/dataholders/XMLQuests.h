#pragma once

#include "aion/gameserver/dataholders/XMLQuests.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.XMLQuests. @author MrPoke */
class XMLQuests : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/XMLQuests.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
