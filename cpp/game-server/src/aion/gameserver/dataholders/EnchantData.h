#pragma once

#include "aion/gameserver/dataholders/EnchantData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.EnchantData. @author xTz */
class EnchantData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/EnchantData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
