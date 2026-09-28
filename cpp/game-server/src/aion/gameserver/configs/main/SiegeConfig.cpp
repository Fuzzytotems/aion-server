#include "aion/gameserver/configs/main/SiegeConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::configs::main {

void SiegeConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.siege.enable", SIEGE_ENABLED, "true");
	AION_BIND(p, "gameserver.siege.assault.enable", BALAUR_AUTO_ASSAULT, "false");
	AION_BIND(p, "gameserver.siege.assault.rate", BALAUR_ASSAULT_RATE, "1");
	AION_BIND(p, "gameserver.moltenus.time", MOLTENUS_SPAWN_SCHEDULE, "0 0 22 ? * SUN");
	AION_BIND(p, "gameserver.siege.health.multiplier.fortress", FORTRESS_PROTECTOR_HEALTH_MULTIPLIER, "1");
	AION_BIND(p, "gameserver.siege.health.multiplier.artifact", ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER, "1");
	AION_BIND(p, "gameserver.siege.health.multiplier.base", BASE_PROTECTOR_HEALTH_MULTIPLIER, "1");
	AION_BIND(p, "gameserver.siege.difficulty.multiplier", SIEGE_DIFFICULTY_MULTIPLIER, "1");
	AION_BIND(p, "gameserver.siege.panesterra.maxplayers", PANESTERRA_MAX_PLAYERS_PER_TEAM, "100");
	AION_BIND(p, "gameserver.siege.panesterra.ahserion.maxplayers", AHSERION_MAX_PLAYERS_PER_TEAM, "100");
	AION_BIND(p, "gameserver.siege.panesterra.ahserion.time", AHSERION_START_SCHEDULE, "0 50 18 ? * SUN");
	AION_BIND(p, "gameserver.siege.legion.gp.cap_per_member", LEGION_GP_CAP_PER_MEMBER, "200");
	AION_BIND(p, "gameserver.siege.door.repair.heal.percent", DOOR_REPAIR_HEAL_PERCENT, "0.01");
	AION_BIND(p, "gameserver.siege.reward.balaur.victory", SIEGE_REWARD_BALAUR_VICTORY, "false");
	AION_BIND(p, "gameserver.siege.ignore_staff_on_location_clear", IGNORE_STAFF_ON_LOCATION_CLEAR, "false");
}

} // namespace aion::gameserver::configs::main
