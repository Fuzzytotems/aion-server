#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/services/mail/SiegeResult.h"

namespace aion::gameserver::services::mail {

/** Companion of the generated enum SiegeResult (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the value constructor argument, in ordinal order (SiegeResult.java:9-14) */
inline constexpr std::array<int32_t, 6> SIEGE_RESULT_IDS{{
	0, // DEFENCE
	1, // OCCUPY
	2, // PROTECT
	3, // DEFENDER
	4, // EMPTY
	5, // FAIL
}};
static_assert(static_cast<size_t>(SiegeResult::FAIL) + 1 == SIEGE_RESULT_IDS.size(), "one entry per SiegeResult constant");
} // namespace detail

/** Java: SiegeResult.getId() */
constexpr int32_t getId(SiegeResult result) noexcept {
	return detail::SIEGE_RESULT_IDS[static_cast<size_t>(result)];
}

} // namespace aion::gameserver::services::mail
