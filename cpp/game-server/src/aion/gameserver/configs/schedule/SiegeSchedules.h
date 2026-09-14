#pragma once

#include "aion/gameserver/configs/schedule/SiegeSchedules.xml.h"

namespace aion::gameserver::configs::schedule {

/** Java com.aionemu.gameserver.configs.schedule.SiegeSchedules. @author SoulKeeper, Source, Estrayl */
class SiegeSchedules : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/SiegeSchedules.xml.inc"
public:
};

/** Java com.aionemu.gameserver.configs.schedule.SiegeSchedules.SiegeSchedule. */
class SiegeSchedules::SiegeSchedule : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/SiegeSchedules_SiegeSchedule.xml.inc"
public:
};

/** Java com.aionemu.gameserver.configs.schedule.SiegeSchedules.AgentFight. */
class SiegeSchedules::AgentFight : public ::aion::gameserver::configs::schedule::SiegeSchedules::SiegeSchedule {
#include "aion/gameserver/configs/schedule/SiegeSchedules_AgentFight.xml.inc"
public:
};

/** Java com.aionemu.gameserver.configs.schedule.SiegeSchedules.Fortress. */
class SiegeSchedules::Fortress : public ::aion::gameserver::configs::schedule::SiegeSchedules::SiegeSchedule {
#include "aion/gameserver/configs/schedule/SiegeSchedules_Fortress.xml.inc"
public:
};

} // namespace aion::gameserver::configs::schedule
