#pragma once

#include "aion/gameserver/dataholders/TradeListData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TradeListData. @author Luno */
class TradeListData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TradeListData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
