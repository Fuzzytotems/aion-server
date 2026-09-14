#pragma once

#include "aion/gameserver/dataholders/MultiReturnItemData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.MultiReturnItemData. @author ginho1 */
class MultiReturnItemData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MultiReturnItemData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
