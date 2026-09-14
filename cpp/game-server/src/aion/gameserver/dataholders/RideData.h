#pragma once

#include "aion/gameserver/dataholders/RideData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.RideData. @author Rolandas */
class RideData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RideData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
