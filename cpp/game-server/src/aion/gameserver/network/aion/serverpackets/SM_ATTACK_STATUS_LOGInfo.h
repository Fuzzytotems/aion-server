#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"

namespace aion::gameserver::network::aion::serverpackets {

/** Companion of the generated nested enum SM_ATTACK_STATUS.LOG (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

namespace detail {
/** SM_ATTACK_STATUS.LOG values in ordinal order */
inline constexpr std::array<int32_t, 16> ATTACK_STATUS_LOG_VALUES{{
	1,   // SPELLATK
	3,   // HEAL
	4,   // MPHEAL
	21,  // CASEHEAL
	23,  // SKILLLATKDRAININSTANT
	24,  // SPELLATKDRAININSTANT
	25,  // POISON
	26,  // BLEED
	93,  // PROCATKINSTANT: changed in 4.5
	97,  // DELAYEDSPELLATKINSTANT: changed in 4.5
	112, // MAGICCOUNTERATK
	132, // SPELLATKDRAIN: changed in 4.5
	134, // FPHEAL: changed in 4.5
	137, // FPATTACK
	141, // MPATTACK
	191, // REGULAR: 4.8
}};
static_assert(static_cast<size_t>(SM_ATTACK_STATUS_LOG::REGULAR) + 1 == ATTACK_STATUS_LOG_VALUES.size(), "one entry per LOG constant");
} // namespace detail

/** Java: SM_ATTACK_STATUS.LOG.getValue() */
constexpr int32_t getValue(SM_ATTACK_STATUS_LOG log) noexcept {
	return detail::ATTACK_STATUS_LOG_VALUES[static_cast<size_t>(log)];
}

} // namespace aion::gameserver::network::aion::serverpackets
