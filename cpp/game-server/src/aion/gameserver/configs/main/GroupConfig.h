#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.GroupConfig
 */
struct GroupConfig {
	/** Group remove time */
	static inline std::atomic<int32_t> GROUP_REMOVE_TIME{0};

	/** Group max distance */
	static inline std::atomic<int32_t> GROUP_MAX_DISTANCE{0};

	/** Enable Group Invite Other Faction */
	static inline std::atomic<bool> GROUP_INVITEOTHERFACTION{false};

	/** Alliance remove time */
	static inline std::atomic<int32_t> ALLIANCE_REMOVE_TIME{0};

	/** Enable Alliance Invite Other Faction */
	static inline std::atomic<bool> ALLIANCE_INVITEOTHERFACTION{false};

	/** Allow applying for or registering instance groups in the Find Group window even if you're not at the instance entrance (like in version 6.2+) */
	static inline std::atomic<bool> FORM_INSTANCE_GROUP_ANYWHERE{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
