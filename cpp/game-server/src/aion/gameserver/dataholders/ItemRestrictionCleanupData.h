#pragma once

#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemRestrictionCleanupData. @author KID */
class ItemRestrictionCleanupData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
