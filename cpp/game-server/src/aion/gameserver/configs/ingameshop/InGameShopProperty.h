#pragma once

#include "aion/gameserver/configs/ingameshop/InGameShopProperty.xml.h"

namespace aion::gameserver::configs::ingameshop {

/** Java com.aionemu.gameserver.configs.ingameshop.InGameShopProperty. @author xTz */
class InGameShopProperty : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/ingameshop/InGameShopProperty.xml.inc"
public:
};

} // namespace aion::gameserver::configs::ingameshop
