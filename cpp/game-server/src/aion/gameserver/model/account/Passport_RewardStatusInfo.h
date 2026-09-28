#pragma once

#include <cstdint>

#include "aion/gameserver/model/account/Passport_RewardStatus.h"

namespace aion::gameserver::model::account {

/** Companion of the generated nested enum Passport.RewardStatus (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

/** Java: RewardStatus.getId() - UPCOMING 0, AVAILABLE 1, TAKEN 2, EXPIRED 3 (equal to the ordinal) */
constexpr int32_t getId(Passport_RewardStatus status) noexcept {
	return static_cast<int32_t>(status);
}

} // namespace aion::gameserver::model::account
