#pragma once

#include "aion/gameserver/dataholders/ItemPurificationData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ItemPurificationData. @author Ranastic, Navyan */
class ItemPurificationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemPurificationData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
