#pragma once

#include "aion/gameserver/dataholders/CuringObjectsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.CuringObjectsData. @author xTz */
class CuringObjectsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CuringObjectsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
