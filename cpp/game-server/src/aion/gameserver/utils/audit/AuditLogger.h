#pragma once

#include <string_view>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/utils/audit/fwd.h"

namespace aion::gameserver::utils::audit {

/**
 * C++: a static-only class (fieldmap K5); the "AUDIT_LOG" logger lives in the .cpp.
 *
 * @author MrPoke, Neon
 */
class AuditLogger {
public:
	AuditLogger() = delete;

	/**
	 * Logs message, if audit log is enabled.<br>
	 * Notifies permitted online staff members.<br>
	 * Automatically punishes player, if punishments are enabled.
	 */
	static void log(model::gameobjects::player::Player& player, std::string_view message);
};

} // namespace aion::gameserver::utils::audit
