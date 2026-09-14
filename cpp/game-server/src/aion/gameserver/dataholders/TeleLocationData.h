#pragma once

#include "aion/gameserver/dataholders/TeleLocationData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TeleLocationData. @author orz */
class TeleLocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TeleLocationData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
