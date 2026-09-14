#pragma once

#include "aion/gameserver/dataholders/AtreianPassportData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.AtreianPassportData. @author Alcapwnd, ViAl */
class AtreianPassportData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AtreianPassportData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
