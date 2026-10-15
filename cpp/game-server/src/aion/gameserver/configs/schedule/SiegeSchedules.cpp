#include "aion/gameserver/configs/schedule/SiegeSchedules.h"

#include "aion/gameserver/configs/schedule/SiegeSchedules.bind.h"
#include "aion/gameserver/dataholders/detail/JaxbDeserialize.h"

namespace aion::gameserver::configs::schedule {

std::unique_ptr<SiegeSchedules> SiegeSchedules::load() {
	return dataholders::detail::deserializeFile<SiegeSchedules>("./config/schedule/siege_schedule.xml",
	                                                            "com.aionemu.gameserver.configs.schedule.SiegeSchedules");
}

} // namespace aion::gameserver::configs::schedule
