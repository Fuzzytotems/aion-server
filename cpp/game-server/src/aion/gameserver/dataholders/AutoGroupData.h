#pragma once

#include "aion/gameserver/dataholders/AutoGroupData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AutoGroupData. */
class AutoGroupData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AutoGroupData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
