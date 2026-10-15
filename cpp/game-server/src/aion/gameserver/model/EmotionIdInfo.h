#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/EmotionId.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum EmotionId (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). Some
 * emotions are for NPCs (like ANGRY, THANK, THINK, SURPRISE).
 */

namespace detail {
inline constexpr std::array<int32_t, 22> EMOTION_ID_IDS{{
	0,   // NONE
	1,   // LAUGH
	2,   // ANGRY
	3,   // SAD
	5,   // POINT
	6,   // YES
	7,   // NO
	8,   // VICTORY
	11,  // CLAP
	12,  // SIGH
	13,  // SURPRISE
	14,  // COMFORT
	15,  // THANK
	16,  // BEG
	17,  // BLUSH
	28,  // SMILE
	29,  // SALUTE
	30,  // PANIC
	31,  // SORRY
	33,  // THINK
	34,  // DISLIKE
	128, // STAND: All action NPCs having animation quest_actstanding
}};
static_assert(static_cast<size_t>(EmotionId::STAND) + 1 == EMOTION_ID_IDS.size(), "one entry per EmotionId constant");
} // namespace detail

/** Java: EmotionId.id() */
constexpr int32_t id(EmotionId emotion) noexcept {
	return detail::EMOTION_ID_IDS[static_cast<size_t>(emotion)];
}

} // namespace aion::gameserver::model
