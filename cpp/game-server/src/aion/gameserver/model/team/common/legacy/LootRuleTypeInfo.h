#pragma once

#include <cstdint>

#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"

namespace aion::gameserver::model::team::common::legacy {

/** Companion of the generated enum LootRuleType (docs/design/static-data.md §2.5). */

/** Java: LootRuleType.getId() - FREEFORALL 0, ROUNDROBIN 1, LEADER 2 (equal to the ordinal) */
constexpr int32_t getId(LootRuleType rule) noexcept {
	return static_cast<int32_t>(rule);
}

} // namespace aion::gameserver::model::team::common::legacy
