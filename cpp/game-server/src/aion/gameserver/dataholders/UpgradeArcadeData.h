#pragma once

#include "aion/gameserver/dataholders/UpgradeArcadeData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.UpgradeArcadeData. @author ginho1 */
class UpgradeArcadeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/UpgradeArcadeData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
