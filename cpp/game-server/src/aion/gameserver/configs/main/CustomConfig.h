#pragma once

#include <atomic>
#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.CustomConfig
 */
struct CustomConfig {
	/** Enables challenge tasks */
	static inline std::atomic<bool> CHALLENGE_TASKS_ENABLED{false};

	/** Announce when a player successfully enchants an item to +15 or +20 */
	static inline std::atomic<bool> ENABLE_ENCHANT_ANNOUNCE{false};

	/** Enable speaking between factions */
	static inline std::atomic<bool> SPEAKING_BETWEEN_FACTIONS{false};

	/** Minimum level to use whisper */
	static inline std::atomic<int32_t> LEVEL_TO_WHISPER{0};

	/** Time in days after which an item in broker will be unregistered (client cannot display more than 255 days) */
	static inline std::atomic<int32_t> BROKER_REGISTRATION_EXPIRATION_DAYS{0};

	/** Factions search mode */
	static inline std::atomic<bool> FACTIONS_SEARCH_MODE{false};

	/** list gm when search players */
	static inline std::atomic<bool> SEARCH_GM_LIST{false};

	/** Minimum level to use search */
	static inline std::atomic<int32_t> LEVEL_TO_SEARCH{0};

	/** Allow opposite factions to bind in enemy territories */
	static inline std::atomic<bool> ENABLE_CROSS_FACTION_BINDING{false};

	/** Enable second class change without quest */
	static inline std::atomic<bool> ENABLE_SIMPLE_2NDCLASS{false};

	/** Disable chain trigger rate (chain skill with 100% success) */
	static inline std::atomic<bool> SKILL_CHAIN_DISABLE_TRIGGERRATE{false};

	/** Base Fly Time */
	static inline std::atomic<int32_t> BASE_FLYTIME{0};

	static inline std::atomic<bool> FRIENDLIST_GM_RESTRICT{false};

	/** Friendlist size */
	static inline std::atomic<int32_t> FRIENDLIST_SIZE{0};

	/** Basic Quest limit size */
	static inline std::atomic<int32_t> BASIC_QUEST_SIZE_LIMIT{0};

	/** Total number of allowed cube expansions */
	static inline std::atomic<int32_t> CUBE_EXPANSION_LIMIT{0};

	/** Npc Cube Expands limit size */
	static inline std::atomic<int32_t> NPC_CUBE_EXPANDS_SIZE_LIMIT{0};

	/** Enable Kinah cap */
	static inline std::atomic<bool> ENABLE_KINAH_CAP{false};

	/** Kinah cap value */
	static inline std::atomic<int64_t> KINAH_CAP_VALUE{0};

	/** Enable AP cap */
	static inline std::atomic<bool> ENABLE_AP_CAP{false};

	/** AP cap value */
	static inline std::atomic<int64_t> AP_CAP_VALUE{0};

	/** Enable no AP in mentored group. */
	static inline std::atomic<bool> MENTOR_GROUP_AP{false};

	/** .faction cfg */
	static inline std::atomic<int32_t> FACTION_USE_PRICE{0};

	static inline std::atomic<bool> FACTION_CMD_CHANNEL{false};

	static inline std::atomic<bool> FACTION_CHAT_CHANNEL{false};

	/** Time in milliseconds in which players are limited for killing one player */
	static inline std::atomic<int64_t> PVP_DAY_DURATION{0};

	/** Allowed Kills in configuered time for full AP. Move to separate config when more pvp options. */
	static inline std::atomic<int32_t> MAX_DAILY_PVP_KILLS{0};

	/** Add a reward to player for pvp kills */
	static inline std::atomic<bool> ENABLE_KILL_REWARD{false};

	/** Keep buffs when getting killed in Sanctum's Coliseum or Pandaemonium's Triniel Coliseum */
	static inline std::atomic<bool> KEEP_BUFFS_IN_COLISEUM{false};

	/** Enable one kisk restriction */
	static inline std::atomic<bool> ENABLE_KISK_RESTRICTION{false};

	static inline std::atomic<bool> RIFT_ENABLED{false};

	static inline std::atomic<int32_t> RIFT_DURATION{0};

	static inline std::atomic<bool> VORTEX_ENABLED{false};

	static inline std::atomic<const services::cron::CronExpression*> VORTEX_BRUSTHONIN_SCHEDULE{nullptr};

	static inline std::atomic<const services::cron::CronExpression*> VORTEX_THEOBOMOS_SCHEDULE{nullptr};

	static inline std::atomic<int32_t> VORTEX_DURATION{0};

	static inline std::atomic<bool> CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED{false};

	static inline ConfigValue<std::unordered_set<int32_t>> CONQUEROR_AND_PROTECTOR_WORLDS;

	static inline std::atomic<int32_t> CONQUEROR_AND_PROTECTOR_LEVEL_DIFF{0};

	static inline std::atomic<int32_t> CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL{0};

	static inline std::atomic<int32_t> CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_COUNT{0};

	static inline std::atomic<int32_t> CONQUEROR_AND_PROTECTOR_KILLS_RANK1{0};

	static inline std::atomic<int32_t> CONQUEROR_AND_PROTECTOR_KILLS_RANK2{0};

	static inline std::atomic<int32_t> CONQUEROR_AND_PROTECTOR_KILLS_RANK3{0};

	/** Limits Config */
	static inline std::atomic<bool> LIMITS_ENABLED{false};

	static inline std::atomic<bool> LIMITS_ENABLE_DYNAMIC_CAP{false};

	static inline std::atomic<const services::cron::CronExpression*> LIMITS_UPDATE{nullptr};

	static inline std::atomic<bool> ABYSSXFORM_LOGOUT{false};

	static inline std::atomic<bool> ENABLE_RIDE_RESTRICTION{false};

	/** Enables sell apitems */
	static inline std::atomic<bool> SELLING_APITEMS_ENABLED{false};

	static inline std::atomic<int32_t> CHARACTER_DELETION_TIME_MINUTES{0};

	/** Don't consume potions when already at full HP/MP */
	static inline std::atomic<bool> IGNORE_POTIONS_AT_FULL_HEALTH{false};

	/** Cancel a running item use when the player selects another target, as the retail server does */
	static inline std::atomic<bool> CANCEL_ITEM_USE_ON_TARGET_CHANGE{false};

	/** Custom Reward Packages */
	static inline std::atomic<bool> ENABLE_STARTER_KIT{false};

	static inline std::atomic<bool> PVP_MAP_ENABLED{false};

	static inline std::atomic<float> PVP_MAP_AP_MULTIPLIER{0.0f};

	static inline std::atomic<float> PVP_MAP_PVE_AP_MULTIPLIER{0.0f};

	static inline std::atomic<int32_t> PVP_MAP_RANDOM_BOSS_BASE_RATE{0};

	static inline std::atomic<const services::cron::CronExpression*> PVP_MAP_RANDOM_BOSS_SCHEDULE{nullptr};

	static inline std::atomic<float> GODSTONE_ACTIVATION_RATE{0.0f};

	static inline std::atomic<int32_t> GODSTONE_EVALUATION_COOLDOWN_MILLIS{0};

	/** Count summon-applied abnormal effects for cumulative resist. */
	static inline std::atomic<bool> COUNT_SUMMON_EFFECTS_FOR_CUMULATIVE_RESIST{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
