#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/SellLimit.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model {

/** Companion of the generated enum SellLimit (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions (ADL). */

namespace detail {
/** Java constructor arguments (playerMinLevel, playerMaxLevel, limit) in ordinal order */
struct SellLimitData {
	int32_t playerMinLevel;
	int32_t playerMaxLevel;
	int64_t limit;
};

inline constexpr std::array<SellLimitData, 5> SELL_LIMIT_DATA{{
	{1, 30, 5300047},   // LIMIT_1_30
	{31, 40, 7100047},  // LIMIT_31_40
	{41, 55, 12050047}, // LIMIT_41_55
	{56, 60, 14600047}, // LIMIT_56_60
	{61, 65, 17150047}, // LIMIT_61_65
}};
static_assert(static_cast<size_t>(SellLimit::LIMIT_61_65) + 1 == SELL_LIMIT_DATA.size(), "one entry per SellLimit constant");
} // namespace detail

/**
 * Java: SellLimit.getSellLimit(Player) - the limit of the first entry whose level range contains the account's highest character level, with
 * Rates.SELL_LIMIT applied.
 *
 * @throws NoSuchElementException
 *           if no entry covers the level
 */
int64_t getSellLimit(gameobjects::player::Player& player);

} // namespace aion::gameserver::model
