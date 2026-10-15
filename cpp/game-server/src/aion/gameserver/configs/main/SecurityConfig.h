#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.SecurityConfig
 */
struct SecurityConfig {
	enum class MultiClientingRestrictionMode { NONE, FULL, SAME_FACTION };

	static inline std::atomic<bool> AION_BIN_CHECK{false};

	static inline std::atomic<bool> TELEPORTATION{false};

	static inline std::atomic<bool> SPEEDHACK{false};

	static inline std::atomic<int32_t> SPEEDHACK_COUNTER{0};

	static inline std::atomic<bool> ABNORMAL{false};

	static inline std::atomic<int32_t> ABNORMAL_COUNTER{0};

	static inline std::atomic<int32_t> PUNISH{0};

	/** Check for no-animation hacks (prevents premature skill executions and logs suspicious players to audit log) */
	static inline std::atomic<bool> CHECK_ANIMATIONS{false};

	static inline std::atomic<bool> CAPTCHA_ENABLE{false};

	static inline ConfigValue<std::string> CAPTCHA_APPEAR;

	static inline std::atomic<int32_t> CAPTCHA_APPEAR_RATE{0};

	static inline std::atomic<int32_t> CAPTCHA_EXTRACTION_BAN_TIME{0};

	static inline std::atomic<int32_t> CAPTCHA_EXTRACTION_BAN_ADD_TIME{0};

	static inline std::atomic<int32_t> CAPTCHA_BONUS_FP_TIME{0};

	static inline std::atomic<bool> PASSKEY_ENABLE{false};

	static inline std::atomic<int32_t> PASSKEY_WRONG_MAXCOUNT{0};

	static inline std::atomic<bool> PINGCHECK_KICK{false};

	static inline std::atomic<int32_t> FLOOD_DELAY{0};

	static inline std::atomic<int32_t> FLOOD_MSG{0};

	static inline std::atomic<bool> ENABLE_FLYPATH_VALIDATOR{false};

	static inline std::atomic<int32_t> SURVEY_DELAY{0};

	/**
	 * Restriction mode for multi-clienting:<br>
	 * NONE - Players are allowed to log in multiple accounts per computer<br>
	 * FULL - Players are allowed to log in one account per computer<br>
	 * SAME_FACTION - Players are allowed to log in multiple accounts per computer, but only log in characters of the same faction<br>
	 */
	static inline std::atomic<MultiClientingRestrictionMode> MULTI_CLIENTING_RESTRICTION_MODE{MultiClientingRestrictionMode::NONE};

	/** Comma separated list of MAC addresses that are allowed to log in regardless of the configured restrictions. */
	static inline ConfigValue<std::unordered_set<std::string>> MULTI_CLIENTING_IGNORED_MAC_ADDRESSES;

	/**
	 * If multi-clienting is restricted to the same faction, logging in characters of one faction will be denied until all characters of the opposite
	 * faction have been offline for the specified amount of time.
	 */
	static inline std::atomic<int32_t> MULTI_CLIENTING_FACTION_SWITCH_COOLDOWN_MINUTES{0};

	static inline std::atomic<bool> HDD_SERIAL_LOCK_ENABLE{false};

	static inline std::atomic<bool> HDD_SERIAL_LOCK_UNLOCKED_ACCOUNTS{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
