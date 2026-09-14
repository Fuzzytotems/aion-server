#include "aion/gameserver/configs/main/AutoGroupConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::configs::main {

void AutoGroupConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.autogroup.enable", AUTO_GROUP_ENABLE, "true");
	AION_BIND(p, "gameserver.startTime.enable", START_TIME_ENABLE, "true");
	AION_BIND(p, "gameserver.dredgion.registration_period", DREDGION_REGISTRATION_PERIOD, "60");
	AION_BIND(p, "gameserver.dredgion.time", DREDGION_TIMES, "\"0 0 0,12,20 ? * *\"");
	AION_BIND(p, "gameserver.kamar_battlefield.registration_period", KAMAR_BATTLEFIELD_REGISTRATION_PERIOD, "60");
	AION_BIND(p, "gameserver.kamar_battlefield.time", KAMAR_BATTLEFIELD_TIMES, "\"0 0 0,20 ? * MON,WED,SAT\"");
	AION_BIND(p, "gameserver.engulfed_ophidan_bridge.registration_period", ENGULFED_OPHIDAN_BRIDGE_REGISTRATION_PERIOD, "60");
	AION_BIND(p, "gameserver.engulfed_ophidan_bridge.time", ENGULFED_OPHIDAN_BRIDGE_TIMES, "\"0 0 12,19 ? * *\"");
	AION_BIND(p, "gameserver.iron_wall_warfront.registration_period", IRON_WALL_WARFRONT_REGISTRATION_PERIOD, "60");
	AION_BIND(p, "gameserver.iron_wall_warfront.time", IRON_WALL_WARFRONT_TIMES, "\"0 0 0,12 ? * SUN\"");
	AION_BIND(p, "gameserver.idgel_dome.registration_period", IDGEL_DOME_REGISTRATION_PERIOD, "60");
	AION_BIND(p, "gameserver.idgel_dome.time", IDGEL_DOME_TIMES, "0 0 23 ? * *");
	AION_BIND(p, "gameserver.autogroup.announce_battleground_registrations", ANNOUNCE_BATTLEGROUND_REGISTRATIONS, "false");
}

} // namespace aion::gameserver::configs::main
