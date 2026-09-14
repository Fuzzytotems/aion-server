#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.PlayerTransferConfig
 *
 * @author KID
 */
struct PlayerTransferConfig {
	static inline std::atomic<int64_t> MAX_KINAH{0};

	static inline ConfigValue<std::string> BIND_ELYOS;

	static inline ConfigValue<std::string> BIND_ASMO;

	static inline std::atomic<bool> ALLOW_EMOTIONS{false};

	static inline std::atomic<bool> ALLOW_MOTIONS{false};

	static inline std::atomic<bool> ALLOW_MACRO{false};

	static inline std::atomic<bool> ALLOW_NPCFACTIONS{false};

	static inline std::atomic<bool> ALLOW_PETS{false};

	static inline std::atomic<bool> ALLOW_RECIPES{false};

	static inline std::atomic<bool> ALLOW_SKILLS{false};

	static inline std::atomic<bool> ALLOW_TITLES{false};

	static inline std::atomic<bool> ALLOW_QUESTS{false};

	static inline std::atomic<bool> ALLOW_INV{false};

	static inline std::atomic<bool> ALLOW_WAREHOUSE{false};

	static inline std::atomic<bool> ALLOW_STIGMA{false};

	static inline std::atomic<bool> BLOCK_SAMENAME{false};

	static inline std::atomic<int32_t> REUSE_HOURS{0};

	static inline ConfigValue<std::string> REMOVE_SKILL_LIST;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
