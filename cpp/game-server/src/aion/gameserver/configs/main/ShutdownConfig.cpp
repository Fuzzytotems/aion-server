#include "aion/gameserver/configs/main/ShutdownConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::configs::main {

void ShutdownConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.shutdown.delay", DELAY, "120");
	AION_BIND(p, "gameserver.shutdown.restart_schedule", RESTART_SCHEDULE);
}

} // namespace aion::gameserver::configs::main
