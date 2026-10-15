#pragma once

#include <atomic>
#include <cstdint>
#include <regex>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.LegionConfig
 *
 * @author Simple
 */
struct LegionConfig {
	/** Announcement pattern (checked when announcement is being created) */
	static inline ConfigValue<std::wregex> LEGION_NAME_PATTERN;

	/** Self Intro pattern (checked when self intro is being changed) */
	static inline ConfigValue<std::wregex> SELF_INTRO_PATTERN;

	/** Nickname pattern (checked when nickname is being changed) */
	static inline ConfigValue<std::wregex> NICKNAME_PATTERN;

	/** Sets disband legion time */
	static inline std::atomic<int32_t> LEGION_DISBAND_TIME{0};

	/** Sets required kinah to create a legion */
	static inline std::atomic<int32_t> LEGION_CREATE_REQUIRED_KINAH{0};

	/** Sets required kinah to create emblem */
	static inline std::atomic<int32_t> LEGION_EMBLEM_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 2 */
	static inline std::atomic<int32_t> LEGION_LEVEL2_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 3 */
	static inline std::atomic<int32_t> LEGION_LEVEL3_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 4 */
	static inline std::atomic<int32_t> LEGION_LEVEL4_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 5 */
	static inline std::atomic<int32_t> LEGION_LEVEL5_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 6 */
	static inline std::atomic<int32_t> LEGION_LEVEL6_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 7 */
	static inline std::atomic<int32_t> LEGION_LEVEL7_REQUIRED_KINAH{0};

	/** Sets required kinah to level legion up to 8 */
	static inline std::atomic<int32_t> LEGION_LEVEL8_REQUIRED_KINAH{0};

	/** Sets required amount of members to level legion up to 2 */
	static inline std::atomic<int32_t> LEGION_LEVEL2_REQUIRED_MEMBERS{0};

	/** Sets required amount of members to level legion up to 3 */
	static inline std::atomic<int32_t> LEGION_LEVEL3_REQUIRED_MEMBERS{0};

	/** Sets required amount of members to level legion up to 4 */
	static inline std::atomic<int32_t> LEGION_LEVEL4_REQUIRED_MEMBERS{0};

	/** Sets required amount of members to level legion up to 5 */
	static inline std::atomic<int32_t> LEGION_LEVEL5_REQUIRED_MEMBERS{0};

	/** Sets required amount of members to level legion up to 6 */
	static inline std::atomic<int32_t> LEGION_LEVEL6_REQUIRED_MEMBERS{0};

	/** Sets required amount of members to level legion up to 7 */
	static inline std::atomic<int32_t> LEGION_LEVEL7_REQUIRED_MEMBERS{0};

	/** Sets required amount of members to level legion up to 8 */
	static inline std::atomic<int32_t> LEGION_LEVEL8_REQUIRED_MEMBERS{0};

	/** Sets required amount of abyss point to level legion up to 2 */
	static inline std::atomic<int32_t> LEGION_LEVEL2_REQUIRED_CONTRIBUTION{0};

	/** Sets required amount of abyss point to level legion up to 3 */
	static inline std::atomic<int32_t> LEGION_LEVEL3_REQUIRED_CONTRIBUTION{0};

	/** Sets required amount of abyss point to level legion up to 4 */
	static inline std::atomic<int32_t> LEGION_LEVEL4_REQUIRED_CONTRIBUTION{0};

	/** Sets required amount of abyss point to level legion up to 5 */
	static inline std::atomic<int32_t> LEGION_LEVEL5_REQUIRED_CONTRIBUTION{0};

	/** Sets required amount of abyss point to level legion up to 6 */
	static inline std::atomic<int32_t> LEGION_LEVEL6_REQUIRED_CONTRIBUTION{0};

	/** Sets required amount of abyss point to level legion up to 7 */
	static inline std::atomic<int32_t> LEGION_LEVEL7_REQUIRED_CONTRIBUTION{0};

	/** Sets required amount of abyss point to level legion up to 8 */
	static inline std::atomic<int32_t> LEGION_LEVEL8_REQUIRED_CONTRIBUTION{0};

	/** Sets max members of a level 1 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL1_MAX_MEMBERS{0};

	/** Sets max members of a level 2 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL2_MAX_MEMBERS{0};

	/** Sets max members of a level 3 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL3_MAX_MEMBERS{0};

	/** Sets max members of a level 4 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL4_MAX_MEMBERS{0};

	/** Sets max members of a level 5 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL5_MAX_MEMBERS{0};

	/** Sets max members of a level 6 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL6_MAX_MEMBERS{0};

	/** Sets max members of a level 7 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL7_MAX_MEMBERS{0};

	/** Sets max members of a level 8 legion */
	static inline std::atomic<int32_t> LEGION_LEVEL8_MAX_MEMBERS{0};

	/** Enable/disable Legion Warehouse */
	static inline std::atomic<bool> LEGION_WAREHOUSE{false};

	/** Enable/disable Legion Invite Other Faction */
	static inline std::atomic<bool> LEGION_INVITEOTHERFACTION{false};

	static inline std::atomic<bool> ENABLE_GUILD_TASK_REQ{false};

	/** Enable/Disable legion dominion key requirement */
	static inline std::atomic<bool> REQUIRE_KEY_FOR_STONESPEAR_REACH{false};

	/** Min points to be reached in stonespear reach instance to account for a territory election */
	static inline std::atomic<int32_t> STONESPEAR_REACH_MIN_POINTS_FOR_TERRITORY{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
