#pragma once

#include "aion/gameserver/dataholders/DecomposableItemsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.DecomposableItemsData. @author antness */
class DecomposableItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/DecomposableItemsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
