#pragma once

#include "aion/gameserver/dataholders/WalkerData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.WalkerData. @author KKnD, Rolandas */
class WalkerData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/WalkerData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
