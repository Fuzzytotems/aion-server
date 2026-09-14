#pragma once

#include "aion/gameserver/dataholders/TribeRelationsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TribeRelationsData. @author ATracer */
class TribeRelationsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TribeRelationsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
