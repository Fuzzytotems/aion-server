#pragma once

#include "aion/gameserver/dataholders/CosmeticItemsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.CosmeticItemsData. @author xTz */
class CosmeticItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CosmeticItemsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
