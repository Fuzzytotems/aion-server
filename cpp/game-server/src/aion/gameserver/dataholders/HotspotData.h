#pragma once

#include "aion/gameserver/dataholders/HotspotData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.HotspotData. @author ginho1 */
class HotspotData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/HotspotData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
