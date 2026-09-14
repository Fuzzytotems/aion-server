#pragma once

#include "aion/gameserver/dataholders/CustomDrop.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.CustomDrop. @author ViAl, Neon */
class CustomDrop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CustomDrop.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
