#pragma once

#include "aion/gameserver/configs/schedule/WorldRaidSchedules.xml.h"

namespace aion::gameserver::configs::schedule {

/** Java com.aionemu.gameserver.configs.schedule.WorldRaidSchedules. @author Whoop, Sykra */
class WorldRaidSchedules : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/WorldRaidSchedules.xml.inc"
public:
};

} // namespace aion::gameserver::configs::schedule
