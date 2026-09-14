#pragma once

#include "aion/gameserver/dataholders/PortalLocData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PortalLocData. @author xTz */
class PortalLocData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PortalLocData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
