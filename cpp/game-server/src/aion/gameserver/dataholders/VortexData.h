#pragma once

#include "aion/gameserver/dataholders/VortexData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.VortexData. @author Source */
class VortexData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/VortexData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
