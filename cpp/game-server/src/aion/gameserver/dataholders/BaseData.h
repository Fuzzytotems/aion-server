#pragma once

#include "aion/gameserver/dataholders/BaseData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.BaseData. @author Source, Estrayl */
class BaseData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/BaseData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
