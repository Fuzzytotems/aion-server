#pragma once

#include "aion/gameserver/dataholders/WalkerVersionsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WalkerVersionsData. */
class WalkerVersionsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WalkerVersionsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
