#pragma once

#include "aion/gameserver/dataholders/ItemSetData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemSetData. @author ATracer */
class ItemSetData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemSetData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
