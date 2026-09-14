#pragma once

#include <atomic>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.RatesConfig
 *
 * @author Neon
 */
struct RatesConfig {
	/** Success rates */
	static inline ConfigValue<std::vector<float>> CRAFT_CRIT_CHANCES;

	static inline ConfigValue<std::vector<float>> CRAFT_COMBO_CHANCES;

	static inline ConfigValue<std::vector<float>> MANASTONE_CHANCES;

	static inline ConfigValue<std::vector<float>> ENCHANTMENT_STONE_BASE_CHANCES;

	static inline ConfigValue<std::vector<float>> ENCHANTMENT_STONE_AMPLIFIED_CHANCES;

	static inline ConfigValue<std::vector<float>> TEMPERING_CHANCES;

	/** Modifiers */
	static inline ConfigValue<std::vector<float>> XP_SOLO_RATES;

	static inline ConfigValue<std::vector<float>> XP_GROUP_RATES;

	static inline ConfigValue<std::vector<float>> XP_QUEST_RATES;

	static inline ConfigValue<std::vector<float>> XP_GATHERING_RATES;

	static inline ConfigValue<std::vector<float>> XP_CRAFTING_RATES;

	static inline ConfigValue<std::vector<float>> XP_PVP_RATES;

	static inline ConfigValue<std::vector<float>> SKILL_XP_GATHERING_RATES;

	static inline ConfigValue<std::vector<float>> SKILL_XP_CRAFTING_RATES;

	static inline ConfigValue<std::vector<float>> AP_PVP_RATES;

	static inline ConfigValue<std::vector<float>> AP_PVP_LOSS_RATES;

	static inline ConfigValue<std::vector<float>> AP_PVE_RATES;

	static inline ConfigValue<std::vector<float>> AP_QUEST_RATES;

	static inline ConfigValue<std::vector<float>> AP_DREDGION_RATES;

	static inline ConfigValue<std::vector<float>> GP_RATES;

	static inline ConfigValue<std::vector<float>> DP_PVE_RATES;

	static inline ConfigValue<std::vector<float>> DP_PVP_RATES;

	static inline ConfigValue<std::vector<float>> QUEST_KINAH_RATES;

	static inline ConfigValue<std::vector<float>> DROP_RATES;

	static inline ConfigValue<std::vector<float>> GATHERING_COUNT_RATES;

	static inline ConfigValue<std::vector<float>> PVP_ARENA_DISCIPLINE_REWARD_RATES;

	static inline ConfigValue<std::vector<float>> PVP_ARENA_CHAOS_REWARD_RATES;

	static inline ConfigValue<std::vector<float>> PVP_ARENA_HARMONY_REWARD_RATES;

	static inline ConfigValue<std::vector<float>> PVP_ARENA_GLORY_REWARD_RATES;

	static inline ConfigValue<std::vector<float>> SELL_LIMIT_RATES;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
