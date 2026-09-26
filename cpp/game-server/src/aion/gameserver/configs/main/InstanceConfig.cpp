#include "aion/gameserver/configs/main/InstanceConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void InstanceConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.instance.cooldown_rate", INSTANCE_COOLDOWN_RATE, "1");
	AION_BIND(p, "gameserver.instance.cooldown_rate.excluded_maps", INSTANCE_COOLDOWN_RATE_EXCLUDED_MAPS, "");
	AION_BIND(p, "gameserver.instance.destroy_delay_seconds", INSTANCE_DESTROY_DELAY_SECONDS, "600");
	AION_BIND(p, "gameserver.instance.solo.destroy_delay_seconds", SOLO_INSTANCE_DESTROY_DELAY_SECONDS, "600");
	AION_BIND(p, "gameserver.instance.duel.enable", INSTANCE_DUEL_ENABLE, "true");
	AION_BIND(p, "gameserver.instance.scaling.enable", INSTANCE_SCALING_ENABLE, "false");
	AION_BIND(p, "gameserver.instance.scaling.max_level_diff", INSTANCE_SCALING_MAX_LEVEL_DIFF, "5");
	AION_BIND(p, "gameserver.instance.scaling.npc_min_rating", INSTANCE_SCALING_NPC_MIN_RATING, "ELITE");
	AION_BIND(p, "gameserver.instance.scaling.hp_scale_factor", INSTANCE_SCALING_HP_SCALE_FACTOR, "0.75");
	AION_BIND(p, "gameserver.instance.scaling.hp_floor", INSTANCE_SCALING_HP_FLOOR, "0.5");
	AION_BIND(p, "gameserver.instance.scaling.dmg_scale_factor", INSTANCE_SCALING_DMG_SCALE_FACTOR, "0.5");
	AION_BIND(p, "gameserver.instance.scaling.dmg_floor", INSTANCE_SCALING_DMG_FLOOR, "0.75");
	AION_BIND(p, "gameserver.instance.scaling.excluded_maps", INSTANCE_SCALING_EXCLUDED_MAPS, "");
	AION_BIND(p, "gameserver.instance.handler_directory", HANDLER_DIRECTORY, "./data/handlers/instance");
}

} // namespace aion::gameserver::configs::main
