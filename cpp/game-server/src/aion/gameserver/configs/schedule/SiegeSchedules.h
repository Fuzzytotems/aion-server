#pragma once

#include <memory>

#include "aion/gameserver/configs/schedule/SiegeSchedules.xml.h"

namespace aion::gameserver::configs::schedule {

/** Java com.aionemu.gameserver.configs.schedule.SiegeSchedules. C++: load binds ./config/schedule/siege_schedule.xml like JAXBUtil.deserialize.
 * @author SoulKeeper, Source, Estrayl */
class SiegeSchedules : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/SiegeSchedules.xml.inc"
public:
	/** @throws commons::utils::Exception if the file cannot be read or bound */
	static std::unique_ptr<SiegeSchedules> load();
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
