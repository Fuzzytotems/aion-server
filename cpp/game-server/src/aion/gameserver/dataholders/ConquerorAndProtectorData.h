#pragma once

#include "aion/gameserver/dataholders/ConquerorAndProtectorData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ConquerorAndProtectorData. @author Dtem */
class ConquerorAndProtectorData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
