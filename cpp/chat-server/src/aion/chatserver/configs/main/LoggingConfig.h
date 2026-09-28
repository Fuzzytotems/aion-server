#pragma once

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::chatserver::configs::main {

/**
 * The chat server's logging switches (config/main/logging.properties). Bound once at startup (see configs::Config), read afterwards.
 * <p>
 * Java: com.aionemu.chatserver.configs.main.LoggingConfig
 */
struct LoggingConfig {
	/** Log requests to new channels */
	static inline bool LOG_CHANNEL_REQUEST = false;

	/** Log requests to invalid channels */
	static inline bool LOG_CHANNEL_INVALID = false;

	/** Log Chat */
	static inline bool LOG_CHAT = false;

	/** Log Chat and Save to Database */
	static inline bool LOG_CHAT_TO_DB = false;

	/** Binds the fields above to their property keys (Java: the @Property annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::chatserver::configs::main
