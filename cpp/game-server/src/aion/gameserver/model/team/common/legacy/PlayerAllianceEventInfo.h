#pragma once

#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"

namespace aion::gameserver::model::team::common::legacy {

/** Companion of the generated enum PlayerAllianceEvent (docs/design/static-data.md §2.5). */

/**
 * Java: PlayerAllianceEvent.getId() in ordinal order - LEAVE 0, BANNED 0, MOVEMENT 1, DISCONNECTED 3, JOIN 5, ENTER_OFFLINE 7, UPDATE_EFFECTS 65,
 * RECONNECT 13, ENTER 13, UPDATE 13, MEMBER_GROUP_CHANGE 5, APPOINT_VICE_CAPTAIN 13, DEMOTE_VICE_CAPTAIN 13, APPOINT_CAPTAIN 13
 */
constexpr int32_t getId(PlayerAllianceEvent event) noexcept {
	constexpr int32_t IDS[] = {0, 0, 1, 3, 5, 7, 65, 13, 13, 13, 5, 13, 13, 13};
	return IDS[static_cast<size_t>(event)];
}

} // namespace aion::gameserver::model::team::common::legacy
