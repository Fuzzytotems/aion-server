#include "aion/gameserver/configs/schedule/WorldRaidSchedules.h"

#include "aion/gameserver/configs/schedule/WorldRaidSchedules.bind.h"
#include "aion/gameserver/dataholders/detail/JaxbDeserialize.h"

namespace aion::gameserver::configs::schedule {

std::unique_ptr<WorldRaidSchedules> WorldRaidSchedules::load() {
	return dataholders::detail::deserializeFile<WorldRaidSchedules>("./config/schedule/world_raid_schedule.xml",
	                                                                "com.aionemu.gameserver.configs.schedule.WorldRaidSchedules");
}

} // namespace aion::gameserver::configs::schedule
