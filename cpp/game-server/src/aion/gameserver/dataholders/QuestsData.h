#pragma once

#include "aion/gameserver/dataholders/QuestsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.QuestsData. @author MrPoke */
class QuestsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/QuestsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
