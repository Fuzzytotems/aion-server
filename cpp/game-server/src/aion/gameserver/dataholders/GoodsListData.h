#pragma once

#include "aion/gameserver/dataholders/GoodsListData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.GoodsListData. @author ATracer */
class GoodsListData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GoodsListData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
