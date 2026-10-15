#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.NameConfig
 *
 * @author nrg
 */
struct NameConfig {
	/** Enables custom names usage. */
	static inline std::atomic<bool> ALLOW_CUSTOM_NAMES{false};

	/** Character name pattern (checked when character is being created or renamed) */
	static inline ConfigValue<std::wregex> CHAR_NAME_PATTERN;

	/** Pet name pattern (checked when pet is being adopted or renamed) */
	static inline ConfigValue<std::wregex> PET_NAME_PATTERN;

	/**
	 * Forbidden word sequences (Regex pattern).<br>
	 * Filters names.
	 */
	static inline ConfigValue<std::optional<std::wregex>> FORBIDDEN_SEQUENCE_PATTERN;

	/**
	 * Forbidden words.<br>
	 * Filters names & chat.
	 */
	static inline ConfigValue<std::vector<std::string>> FORBIDDEN_WORDS;

	/** Number of days a name is reserved after renaming. During this time, only the renamed player can (re)adopt this name. */
	static inline std::atomic<int32_t> RESERVE_OLD_NAME_DAYS{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
