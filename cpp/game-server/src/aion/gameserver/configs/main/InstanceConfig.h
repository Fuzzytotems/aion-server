#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <unordered_set>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.InstanceConfig
 */
struct InstanceConfig {
	static inline std::atomic<int32_t> INSTANCE_COOLDOWN_RATE{0};

	static inline ConfigValue<std::unordered_set<int32_t>> INSTANCE_COOLDOWN_RATE_EXCLUDED_MAPS;

	static inline std::atomic<int32_t> INSTANCE_DESTROY_DELAY_SECONDS{0};

	static inline std::atomic<int32_t> SOLO_INSTANCE_DESTROY_DELAY_SECONDS{0};

	static inline std::atomic<bool> INSTANCE_DUEL_ENABLE{false};

	static inline std::atomic<bool> INSTANCE_SCALING_ENABLE{false};

	static inline std::atomic<int32_t> INSTANCE_SCALING_MAX_LEVEL_DIFF{0};

	static inline std::atomic<detail::NpcRating> INSTANCE_SCALING_NPC_MIN_RATING{detail::NpcRating::JUNK};

	static inline std::atomic<float> INSTANCE_SCALING_HP_SCALE_FACTOR{0.0f};

	static inline std::atomic<float> INSTANCE_SCALING_HP_FLOOR{0.0f};

	static inline std::atomic<float> INSTANCE_SCALING_DMG_SCALE_FACTOR{0.0f};

	static inline std::atomic<float> INSTANCE_SCALING_DMG_FLOOR{0.0f};

	static inline ConfigValue<std::unordered_set<int32_t>> INSTANCE_SCALING_EXCLUDED_MAPS;

	/** Location of instance *.java handlers */
	static inline ConfigValue<std::filesystem::path> HANDLER_DIRECTORY;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
