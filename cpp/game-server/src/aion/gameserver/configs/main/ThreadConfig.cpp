#include "aion/gameserver/configs/main/ThreadConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void ThreadConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.thread.base_pool_size", BASE_THREAD_POOL_SIZE, "0");
	AION_BIND(p, "gameserver.thread.scheduled_pool_size", SCHEDULED_THREAD_POOL_SIZE, "0");
	AION_BIND(p, "gameserver.thread.runtime", MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING, "5000");
	AION_BIND(p, "gameserver.thread.usepriority", USE_PRIORITIES, "false");
}

} // namespace aion::gameserver::configs::main
