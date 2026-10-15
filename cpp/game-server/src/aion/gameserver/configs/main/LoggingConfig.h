#pragma once

#include <atomic>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.LoggingConfig
 */
struct LoggingConfig {
	/** Logging */
	static inline std::atomic<bool> LOG_AUDIT{false};

	static inline std::atomic<bool> LOG_CRAFT{false};

	static inline std::atomic<bool> LOG_GMAUDIT{false};

	static inline std::atomic<bool> LOG_GENERAL_CHATS{false};

	static inline std::atomic<bool> LOG_PRIVATE_CHATS{false};

	static inline std::atomic<bool> LOG_ITEM{false};

	static inline std::atomic<bool> LOG_KILL{false};

	static inline std::atomic<bool> LOG_PL{false};

	static inline std::atomic<bool> LOG_MAIL{false};

	static inline std::atomic<bool> LOG_PLAYER_EXCHANGE{false};

	static inline std::atomic<bool> LOG_BROKER_EXCHANGE{false};

	static inline std::atomic<bool> LOG_SIEGE{false};

	static inline std::atomic<bool> LOG_SYSMAIL{false};

	static inline std::atomic<bool> LOG_HOUSE_AUCTION{false};

	static inline std::atomic<bool> LOG_TAMPERING{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
