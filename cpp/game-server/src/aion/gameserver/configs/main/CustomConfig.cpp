#include "aion/gameserver/configs/main/CustomConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::configs::main {

void CustomConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.challenge.tasks.enabled", CHALLENGE_TASKS_ENABLED, "false");
	AION_BIND(p, "gameserver.enchant.announce.enable", ENABLE_ENCHANT_ANNOUNCE, "true");
	AION_BIND(p, "gameserver.chat.factions.enable", SPEAKING_BETWEEN_FACTIONS, "false");
	AION_BIND(p, "gameserver.chat.whisper.level", LEVEL_TO_WHISPER, "10");
	AION_BIND(p, "gameserver.broker.registration_expiration_days", BROKER_REGISTRATION_EXPIRATION_DAYS, "8");
	AION_BIND(p, "gameserver.search.factions.mode", FACTIONS_SEARCH_MODE, "false");
	AION_BIND(p, "gameserver.search.gm.list", SEARCH_GM_LIST, "false");
	AION_BIND(p, "gameserver.search.player.level", LEVEL_TO_SEARCH, "10");
	AION_BIND(p, "gameserver.cross.faction.binding", ENABLE_CROSS_FACTION_BINDING, "false");
	AION_BIND(p, "gameserver.simple.secondclass.enable", ENABLE_SIMPLE_2NDCLASS, "false");
	AION_BIND(p, "gameserver.skill.chain.disable_triggerrate", SKILL_CHAIN_DISABLE_TRIGGERRATE, "false");
	AION_BIND(p, "gameserver.base.flytime", BASE_FLYTIME, "60");
	AION_BIND(p, "gameserver.friendlist.gm_restrict", FRIENDLIST_GM_RESTRICT, "false");
	AION_BIND(p, "gameserver.friendlist.size", FRIENDLIST_SIZE, "90");
	AION_BIND(p, "gameserver.basic.questsize.limit", BASIC_QUEST_SIZE_LIMIT, "40");
	AION_BIND(p, "gameserver.cube.expansion_limit", CUBE_EXPANSION_LIMIT, "11");
	AION_BIND(p, "gameserver.npcexpands.limit", NPC_CUBE_EXPANDS_SIZE_LIMIT, "5");
	AION_BIND(p, "gameserver.enable.kinah.cap", ENABLE_KINAH_CAP, "false");
	AION_BIND(p, "gameserver.kinah.cap.value", KINAH_CAP_VALUE, "999999999");
	AION_BIND(p, "gameserver.enable.ap.cap", ENABLE_AP_CAP, "false");
	AION_BIND(p, "gameserver.ap.cap.value", AP_CAP_VALUE, "1000000");
	AION_BIND(p, "gameserver.noap.mentor.group", MENTOR_GROUP_AP, "false");
	AION_BIND(p, "gameserver.faction.price", FACTION_USE_PRICE, "10000");
	AION_BIND(p, "gameserver.faction.cmdchannel", FACTION_CMD_CHANNEL, "true");
	AION_BIND(p, "gameserver.faction.chatchannels", FACTION_CHAT_CHANNEL, "false");
	AION_BIND(p, "gameserver.pvp.dayduration", PVP_DAY_DURATION, "86400000");
	AION_BIND(p, "gameserver.pvp.maxkills", MAX_DAILY_PVP_KILLS, "5");
	AION_BIND(p, "gameserver.kill.reward.enable", ENABLE_KILL_REWARD, "false");
	AION_BIND(p, "gameserver.coliseum.keep_buffs", KEEP_BUFFS_IN_COLISEUM, "false");
	AION_BIND(p, "gameserver.kisk.restriction.enable", ENABLE_KISK_RESTRICTION, "true");
	AION_BIND(p, "gameserver.rift.enable", RIFT_ENABLED, "true");
	AION_BIND(p, "gameserver.rift.duration", RIFT_DURATION, "1");
	AION_BIND(p, "gameserver.vortex.enable", VORTEX_ENABLED, "true");
	AION_BIND(p, "gameserver.vortex.brusthonin.schedule", VORTEX_BRUSTHONIN_SCHEDULE, "0 0 16 ? * SAT");
	AION_BIND(p, "gameserver.vortex.theobomos.schedule", VORTEX_THEOBOMOS_SCHEDULE, "0 0 16 ? * SUN");
	AION_BIND(p, "gameserver.vortex.duration", VORTEX_DURATION, "1");
	AION_BIND(p, "gameserver.cp.enable", CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED, "true");
	AION_BIND(p, "gameserver.cp.worlds", CONQUEROR_AND_PROTECTOR_WORLDS,
	          "210020000,210040000,210050000,210070000,220020000,220040000,220070000,220080000");
	AION_BIND(p, "gameserver.cp.level.diff", CONQUEROR_AND_PROTECTOR_LEVEL_DIFF, "5");
	AION_BIND(p, "gameserver.cp.kills.decrease_interval_minutes", CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL, "10");
	AION_BIND(p, "gameserver.cp.kills.decrease_count", CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_COUNT, "1");
	AION_BIND(p, "gameserver.cp.kills.rank1", CONQUEROR_AND_PROTECTOR_KILLS_RANK1, "1");
	AION_BIND(p, "gameserver.cp.kills.rank2", CONQUEROR_AND_PROTECTOR_KILLS_RANK2, "10");
	AION_BIND(p, "gameserver.cp.kills.rank3", CONQUEROR_AND_PROTECTOR_KILLS_RANK3, "20");
	AION_BIND(p, "gameserver.limits.enable", LIMITS_ENABLED, "true");
	AION_BIND(p, "gameserver.limits.enable_dynamic_cap", LIMITS_ENABLE_DYNAMIC_CAP, "false");
	AION_BIND(p, "gameserver.limits.update", LIMITS_UPDATE, "0 0 0 ? * *");
	AION_BIND(p, "gameserver.abyssxform.afterlogout", ABYSSXFORM_LOGOUT, "false");
	AION_BIND(p, "gameserver.ride.restriction.enable", ENABLE_RIDE_RESTRICTION, "true");
	AION_BIND(p, "gameserver.selling.apitems.enabled", SELLING_APITEMS_ENABLED, "true");
	AION_BIND(p, "character.deletion.time.minutes", CHARACTER_DELETION_TIME_MINUTES, "5");
	AION_BIND(p, "gameserver.items.ignore_potions_at_full_health", IGNORE_POTIONS_AT_FULL_HEALTH, "false");
	AION_BIND(p, "gameserver.items.cancel_use_on_target_change", CANCEL_ITEM_USE_ON_TARGET_CHANGE, "true");
	AION_BIND(p, "gameserver.custom.starter_kit.enable", ENABLE_STARTER_KIT, "false");
	AION_BIND(p, "gameserver.pvpmap.enable", PVP_MAP_ENABLED, "false");
	AION_BIND(p, "gameserver.pvpmap.apmultiplier", PVP_MAP_AP_MULTIPLIER, "2");
	AION_BIND(p, "gameserver.pvpmap.pve.apmultiplier", PVP_MAP_PVE_AP_MULTIPLIER, "1");
	AION_BIND(p, "gameserver.pvpmap.random_boss.rate", PVP_MAP_RANDOM_BOSS_BASE_RATE, "40");
	AION_BIND(p, "gameserver.pvpmap.random_boss.time", PVP_MAP_RANDOM_BOSS_SCHEDULE, "0 30 14,18,21 ? * *");
	AION_BIND(p, "gameserver.rates.godstone.activation.rate", GODSTONE_ACTIVATION_RATE, "1.0");
	AION_BIND(p, "gameserver.rates.godstone.evaluation.cooldown_millis", GODSTONE_EVALUATION_COOLDOWN_MILLIS, "750");
	AION_BIND(p, "gameserver.pvp.cumulative_resist.count_summon_effects", COUNT_SUMMON_EFFECTS_FOR_CUMULATIVE_RESIST, "false");
}

} // namespace aion::gameserver::configs::main
