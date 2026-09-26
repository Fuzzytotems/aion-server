#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/utils/audit/fwd.h"

namespace aion::gameserver::utils::audit {

/**
 * C++: a static-only class (fieldmap K5). Java's `protected static` punishment (called by AuditLogger in the same package) is private with
 * AuditLogger as friend.
 *
 * @author synchro2
 */
class AutoBan {
public:
	AutoBan() = delete;

private:
	// TODO merge with AntiHackService punishment system / rework
	static void punishment(model::gameobjects::player::Player& player);

	friend class AuditLogger;
};

} // namespace aion::gameserver::utils::audit
