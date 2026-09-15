#pragma once

#include <memory>

#include "aion/gameserver/configs/schedule/WorldRaidSchedules.xml.h"

namespace aion::gameserver::configs::schedule {

/** Java com.aionemu.gameserver.configs.schedule.WorldRaidSchedules. C++: load binds ./config/schedule/world_raid_schedule.xml like
 * JAXBUtil.deserialize. @author Whoop, Sykra */
class WorldRaidSchedules : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/schedule/WorldRaidSchedules.xml.inc"
public:
	/** @throws commons::utils::Exception if the file cannot be read or bound */
	static std::unique_ptr<WorldRaidSchedules> load();
};

} // namespace aion::gameserver::configs::schedule
