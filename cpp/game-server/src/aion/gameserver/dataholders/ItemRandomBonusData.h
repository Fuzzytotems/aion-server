#pragma once

#include "aion/gameserver/dataholders/ItemRandomBonusData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemRandomBonusData. @author Rolandas */
class ItemRandomBonusData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemRandomBonusData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
