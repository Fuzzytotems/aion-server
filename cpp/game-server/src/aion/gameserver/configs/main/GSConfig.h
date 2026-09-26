#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.GSConfig
 */
struct GSConfig {
	/**
	 * Server country code (the client checks it against its cc start parameter)<br>
	 * 0=KR, 1=NA, 2=EU, 4=JP, 5=CN, 6=TW, 7=RU, 99=Region free (allows any client, but client will limit character names to 10 characters)
	 */
	static inline std::atomic<int32_t> SERVER_COUNTRY_CODE{0};

	/** Players Max Level */
	static inline std::atomic<int32_t> PLAYER_MAX_LEVEL{0};

	/**
	 * Time Zone
	 * <p>
	 * An empty value is the system time zone. nullptr (Java: null) until the configuration is loaded.
	 */
	static inline std::atomic<const std::chrono::time_zone*> TIME_ZONE_ID{nullptr};

	/** Enable connection with CS (ChatServer) */
	static inline std::atomic<bool> ENABLE_CHAT_SERVER{false};

	/** Min. required level to write in CS channels */
	static inline std::atomic<int8_t> CHAT_SERVER_MIN_LEVEL{0};

	/** Character creation */
	static inline std::atomic<int32_t> CHARACTER_CREATION_MODE{0};

	static inline std::atomic<int32_t> CHARACTER_LIMIT_COUNT{0};

	static inline std::atomic<int32_t> CHARACTER_FACTION_LIMITATION_MODE{0};

	static inline std::atomic<bool> ENABLE_RATIO_LIMITATION{false};

	static inline std::atomic<int32_t> RATIO_MIN_VALUE{0};

	static inline std::atomic<int32_t> RATIO_MIN_REQUIRED_LEVEL{0};

	static inline std::atomic<int32_t> RATIO_MIN_CHARACTERS_COUNT{0};

	static inline std::atomic<int32_t> RATIO_HIGH_PLAYER_COUNT_DISABLING{0};

	/** Misc */
	static inline std::atomic<int32_t> CHARACTER_REENTRY_TIME{0};

	/** Minimum time in milliseconds between two skill casts. The game client will enforce wait times accordingly. */
	static inline std::atomic<int32_t> MIN_SKILL_CAST_INTERVAL_MILLIS{0};

	static inline std::atomic<int32_t> ITEM_WRAP_LIMIT{0};

	static inline std::atomic<bool> ENABLE_WEB_REWARDS{false};

	static inline std::atomic<bool> ANALYZE_QUESTHANDLERS{false};

	/** Location of quest *.java handlers */
	static inline ConfigValue<std::filesystem::path> QUEST_HANDLER_DIRECTORY;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
