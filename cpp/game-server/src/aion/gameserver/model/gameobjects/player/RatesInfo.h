#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Companion of the generated enum Rates (docs/design/static-data.md §2.5): the constant-specific calcResult bodies and the static methods of the
 * Java enum as free functions found by ADL (`calcResult(Rates::XP_QUEST, player, xp)` for Java `Rates.XP_QUEST.calcResult(player, xp)`). The
 * rates are read from RatesConfig snapshots on every call, so reloaded configs apply at once.
 */

/** Java: rates.calcResult(Player player, long value), the constant-specific body of the Rates constant */
int64_t calcResult(Rates rates, Player& player, int64_t value);

/**
 * Java: rates.calcResult(Player player, int value): the long result as an int; a result outside the int range is logged
 * ("<name> result is too large for <player>: <result>") and the given value is returned instead.
 */
int32_t calcResult(Rates rates, Player& player, int32_t value);

/** Java: Rates.get(player, membershipRates): the rate for the player, selected by his account membership (1 and a warning if there is none) */
float get(Player& player, const std::vector<float>& membershipRates);

} // namespace aion::gameserver::model::gameobjects::player
