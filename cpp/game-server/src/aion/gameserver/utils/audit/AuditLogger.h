#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
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

	/**
	 * C++ only (header request 5a-pre-4): Java's `log(Player, String)` called with a player that may be null. CM_PING passes
	 * `getConnection().getActivePlayer()` (CM_PING.java:36-41), which is null before the character entered the world. A non-null player takes the
	 * overload above. For null it does what the Java body does with a null player: AutoBan.punishment(null) throws NullPointerException if
	 * punishments are enabled, the audit log line starts with "null", and ChatUtil.charName(null) throws NullPointerException as soon as an
	 * online staff member has the AUDIT_INFO access.
	 */
	static void log(runtime::Ptr<model::gameobjects::player::Player> player, std::string_view message);
};

} // namespace aion::gameserver::utils::audit
