#pragma once

#include "aion/gameserver/dataholders/MotionData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.MotionData. @author kecimis */
class MotionData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MotionData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
