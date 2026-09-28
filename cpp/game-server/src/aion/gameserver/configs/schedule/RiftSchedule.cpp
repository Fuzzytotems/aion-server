#include "aion/gameserver/configs/schedule/RiftSchedule.h"

#include "aion/gameserver/configs/schedule/RiftSchedule.bind.h"
#include "aion/gameserver/dataholders/detail/JaxbDeserialize.h"

namespace aion::gameserver::configs::schedule {

std::unique_ptr<RiftSchedule> RiftSchedule::load() {
	return dataholders::detail::deserializeFile<RiftSchedule>("./config/schedule/rift_schedule.xml",
	                                                          "com.aionemu.gameserver.configs.schedule.RiftSchedule");
}

} // namespace aion::gameserver::configs::schedule
