#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::administration {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.administration.AdminConfig
 *
 * @author ATracer, Neon
 */
struct AdminConfig {
	/**
	 * Custom name tags based on access level. The first entry is the tag for access level 1, then 2, 3 and so on.<br>
	 * Default:
	 *
	 * <pre>
	 * Access level 1 = [empty]
	 * Access level 2 = Junior Dev
	 * Access level 3 = Dev
	 * Access level 4 = Junior Event Master
	 * Access level 5 = Event Master
	 * Access level 6 = Junior Game Master
	 * Access level 7 = Game Master
	 * Access level 8 = Senior Game Master
	 * Access level 9 = Admin
	 * </pre>
	 */
	static inline ConfigValue<std::vector<std::string>> NAME_TAGS;

	/** Admin properties */
	static inline std::atomic<int8_t> UNRESTRICTED_ITEMTRADE{0};

	static inline std::atomic<int8_t> GM_PANEL{0};

	static inline std::atomic<int8_t> GM_SKILLS{0};

	static inline std::atomic<int8_t> FREE_FLIGHT{0};

	static inline std::atomic<int8_t> UNLIMITED_FLIGHT_TIME{0};

	static inline std::atomic<int8_t> AUTO_RES{0};

	static inline std::atomic<int8_t> VIEW_PLAYER_DETAILS{0};

	static inline std::atomic<int8_t> INSTANCE_ENTER_ALL{0};

	static inline std::atomic<int8_t> INSTANCE_OPEN_DOORS{0};

	static inline std::atomic<int8_t> INSTANCE_DOOR_INFO{0};

	static inline std::atomic<int8_t> HOUSE_ENTER_ALL{0};

	static inline std::atomic<int8_t> HOUSE_SHOW_ADDRESS{0};

	static inline std::atomic<int8_t> DIALOG_INFO{0};

	static inline std::atomic<int8_t> ENCHANT_INFO{0};

	static inline std::atomic<int8_t> ZONE_INFO{0};

	static inline std::atomic<int8_t> AUDIT_INFO{0};

	/** Special command permissions */
	static inline std::atomic<int8_t> CMD_QUEST_ADV_PARAMS{0};

	/** Login/logout options */
	static inline ConfigValue<std::vector<std::string>> LOGIN_EXECUTE_COMMANDS;

	static inline std::atomic<int8_t> REVISION_INFO_ON_LOGIN{0};

	static inline ConfigValue<std::vector<std::string>> ANNOUNCE_LEVELS;

	static inline std::atomic<bool> ANNOUNCE_LOGIN_TO_ALL_PLAYERS{false};

	static inline std::atomic<bool> ANNOUNCE_LOGOUT_TO_ALL_PLAYERS{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::administration
