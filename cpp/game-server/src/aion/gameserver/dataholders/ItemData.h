#pragma once

#include "aion/gameserver/dataholders/ItemData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemData. @author Luno */
class ItemData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
