#pragma once

#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"

namespace aion::gameserver::model::team::common::legacy {

/** Companion of the generated enum GroupEvent (docs/design/static-data.md §2.5). */

/** Java: GroupEvent.getId() - LEAVE 0, MOVEMENT 1, DISCONNECTED 3, JOIN 5, ENTER_OFFLINE 7, ENTER 13, UPDATE 13, UPDATE_EFFECTS 65 */
constexpr int32_t getId(GroupEvent event) noexcept {
	constexpr int32_t IDS[] = {0, 1, 3, 5, 7, 13, 13, 65};
	return IDS[static_cast<size_t>(event)];
}

} // namespace aion::gameserver::model::team::common::legacy
