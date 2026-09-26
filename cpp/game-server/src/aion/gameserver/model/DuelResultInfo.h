#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/DuelResult.h"

namespace aion::gameserver::model {

/** Companion of the generated enum DuelResult (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
/** Java constructor arguments (msgId, resultId) in ordinal order */
struct DuelResultData {
	int32_t msgId;
	int8_t resultId;
};

inline constexpr std::array<DuelResultData, 3> DUEL_RESULT_DATA{{
	{1300098, 2}, // DUEL_WON
	{1300099, 0}, // DUEL_LOST
	{1300100, 1}, // DUEL_DRAW
}};
static_assert(static_cast<size_t>(DuelResult::DUEL_DRAW) + 1 == DUEL_RESULT_DATA.size(), "one entry per DuelResult constant");
} // namespace detail

/** Java: DuelResult.getMsgId() */
constexpr int32_t getMsgId(DuelResult result) noexcept {
	return detail::DUEL_RESULT_DATA[static_cast<size_t>(result)].msgId;
}

/** Java: DuelResult.getResultId() */
constexpr int8_t getResultId(DuelResult result) noexcept {
	return detail::DUEL_RESULT_DATA[static_cast<size_t>(result)].resultId;
}

} // namespace aion::gameserver::model
