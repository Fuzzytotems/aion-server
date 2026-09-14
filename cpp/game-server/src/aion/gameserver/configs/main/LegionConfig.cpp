#include "aion/gameserver/configs/main/LegionConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void LegionConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.legion.pattern", LEGION_NAME_PATTERN, "[a-zA-Z ]{2,32}");
	AION_BIND(p, "gameserver.legion.selfintropattern", SELF_INTRO_PATTERN, ".{1,32}");
	AION_BIND(p, "gameserver.legion.nicknamepattern", NICKNAME_PATTERN, ".{1,10}");
	AION_BIND(p, "gameserver.legion.disbandtime", LEGION_DISBAND_TIME, "86400");
	AION_BIND(p, "gameserver.legion.creationrequiredkinah", LEGION_CREATE_REQUIRED_KINAH, "10000");
	AION_BIND(p, "gameserver.legion.emblemrequiredkinah", LEGION_EMBLEM_REQUIRED_KINAH, "800000");
	AION_BIND(p, "gameserver.legion.level2requiredkinah", LEGION_LEVEL2_REQUIRED_KINAH, "100000");
	AION_BIND(p, "gameserver.legion.level3requiredkinah", LEGION_LEVEL3_REQUIRED_KINAH, "1000000");
	AION_BIND(p, "gameserver.legion.level4requiredkinah", LEGION_LEVEL4_REQUIRED_KINAH, "5000000");
	AION_BIND(p, "gameserver.legion.level5requiredkinah", LEGION_LEVEL5_REQUIRED_KINAH, "25000000");
	AION_BIND(p, "gameserver.legion.level6requiredkinah", LEGION_LEVEL6_REQUIRED_KINAH, "50000000");
	AION_BIND(p, "gameserver.legion.level7requiredkinah", LEGION_LEVEL7_REQUIRED_KINAH, "75000000");
	AION_BIND(p, "gameserver.legion.level8requiredkinah", LEGION_LEVEL8_REQUIRED_KINAH, "100000000");
	AION_BIND(p, "gameserver.legion.level2requiredmembers", LEGION_LEVEL2_REQUIRED_MEMBERS, "10");
	AION_BIND(p, "gameserver.legion.level3requiredmembers", LEGION_LEVEL3_REQUIRED_MEMBERS, "20");
	AION_BIND(p, "gameserver.legion.level4requiredmembers", LEGION_LEVEL4_REQUIRED_MEMBERS, "30");
	AION_BIND(p, "gameserver.legion.level5requiredmembers", LEGION_LEVEL5_REQUIRED_MEMBERS, "40");
	AION_BIND(p, "gameserver.legion.level6requiredmembers", LEGION_LEVEL6_REQUIRED_MEMBERS, "50");
	AION_BIND(p, "gameserver.legion.level7requiredmembers", LEGION_LEVEL7_REQUIRED_MEMBERS, "60");
	AION_BIND(p, "gameserver.legion.level8requiredmembers", LEGION_LEVEL8_REQUIRED_MEMBERS, "70");
	AION_BIND(p, "gameserver.legion.level2requiredcontribution", LEGION_LEVEL2_REQUIRED_CONTRIBUTION, "0");
	AION_BIND(p, "gameserver.legion.level3requiredcontribution", LEGION_LEVEL3_REQUIRED_CONTRIBUTION, "20000");
	AION_BIND(p, "gameserver.legion.level4requiredcontribution", LEGION_LEVEL4_REQUIRED_CONTRIBUTION, "100000");
	AION_BIND(p, "gameserver.legion.level5requiredcontribution", LEGION_LEVEL5_REQUIRED_CONTRIBUTION, "500000");
	AION_BIND(p, "gameserver.legion.level6requiredcontribution", LEGION_LEVEL6_REQUIRED_CONTRIBUTION, "2500000");
	AION_BIND(p, "gameserver.legion.level7requiredcontribution", LEGION_LEVEL7_REQUIRED_CONTRIBUTION, "12500000");
	AION_BIND(p, "gameserver.legion.level8requiredcontribution", LEGION_LEVEL8_REQUIRED_CONTRIBUTION, "62500000");
	AION_BIND(p, "gameserver.legion.level1maxmembers", LEGION_LEVEL1_MAX_MEMBERS, "30");
	AION_BIND(p, "gameserver.legion.level2maxmembers", LEGION_LEVEL2_MAX_MEMBERS, "60");
	AION_BIND(p, "gameserver.legion.level3maxmembers", LEGION_LEVEL3_MAX_MEMBERS, "90");
	AION_BIND(p, "gameserver.legion.level4maxmembers", LEGION_LEVEL4_MAX_MEMBERS, "120");
	AION_BIND(p, "gameserver.legion.level5maxmembers", LEGION_LEVEL5_MAX_MEMBERS, "150");
	AION_BIND(p, "gameserver.legion.level6maxmembers", LEGION_LEVEL6_MAX_MEMBERS, "180");
	AION_BIND(p, "gameserver.legion.level7maxmembers", LEGION_LEVEL7_MAX_MEMBERS, "210");
	AION_BIND(p, "gameserver.legion.level8maxmembers", LEGION_LEVEL8_MAX_MEMBERS, "240");
	AION_BIND(p, "gameserver.legion.warehouse", LEGION_WAREHOUSE, "true");
	AION_BIND(p, "gameserver.legion.inviteotherfaction", LEGION_INVITEOTHERFACTION, "false");
	AION_BIND(p, "gameserver.legion.task.requirement.enable", ENABLE_GUILD_TASK_REQ, "true");
	AION_BIND(p, "gameserver.legion.require_key_for_stonespear_reach", REQUIRE_KEY_FOR_STONESPEAR_REACH, "true");
	AION_BIND(p, "gameserver.legion.stonespear_reach_min_points", STONESPEAR_REACH_MIN_POINTS_FOR_TERRITORY, "0");
}

} // namespace aion::gameserver::configs::main
