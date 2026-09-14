#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.MembershipConfig
 */
struct MembershipConfig {
	static inline ConfigValue<std::vector<std::string>> MEMBERSHIP_TYPES;

	static inline std::atomic<int8_t> GATHERING_ALLOW_ON_MOUNT{0};

	static inline std::atomic<int8_t> INSTANCES_TITLE_REQ{0};

	static inline std::atomic<int8_t> INSTANCES_RACE_REQ{0};

	static inline std::atomic<int8_t> INSTANCES_LEVEL_REQ{0};

	static inline std::atomic<int8_t> INSTANCES_GROUP_REQ{0};

	static inline std::atomic<int8_t> INSTANCES_QUEST_REQ{0};

	static inline std::atomic<int8_t> INSTANCES_COOLDOWN{0};

	static inline std::atomic<int8_t> EMOTIONS_ALL{0};

	static inline std::atomic<int8_t> STIGMA_SLOT_QUEST{0};

	static inline std::atomic<int8_t> DISABLE_SOULSICKNESS{0};

	static inline std::atomic<int8_t> STIGMA_AUTOLEARN{0};

	static inline std::atomic<int8_t> QUEST_LIMIT_DISABLED{0};

	static inline std::atomic<int8_t> CHARACTER_ADDITIONAL_ENABLE{0};

	static inline std::atomic<int8_t> CHARACTER_ADDITIONAL_COUNT{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
