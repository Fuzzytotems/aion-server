#pragma once

#include "aion/gameserver/dataholders/WarehouseExpandData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WarehouseExpandData. @author spufy */
class WarehouseExpandData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WarehouseExpandData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
