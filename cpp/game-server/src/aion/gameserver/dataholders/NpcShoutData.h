#pragma once

#include "aion/gameserver/dataholders/NpcShoutData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.NpcShoutData. @author Rolandas */
class NpcShoutData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcShoutData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
