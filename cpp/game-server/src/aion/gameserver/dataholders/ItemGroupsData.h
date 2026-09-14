#pragma once

#include "aion/gameserver/dataholders/ItemGroupsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemGroupsData. @author Rolandas */
class ItemGroupsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemGroupsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
