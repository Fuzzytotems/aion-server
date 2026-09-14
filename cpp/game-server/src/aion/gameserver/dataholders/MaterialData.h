#pragma once

#include "aion/gameserver/dataholders/MaterialData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.MaterialData. @author Rolandas */
class MaterialData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MaterialData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
