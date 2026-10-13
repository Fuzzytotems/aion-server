#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/custom/instance/CustomInstanceRankEnum.h"

namespace aion::gameserver::custom::instance {

/**
 * Companion of the generated enum CustomInstanceRankEnum (docs/design/static-data.md §2.5; D12, owner 2026-10-13): the constructor's data
 * and the static methods as free functions found by ADL. The rank icon is Java's char (a UTF-16 code unit); getRankDescription returns UTF-8.
 *
 * @author Estrayl
 */

/** Java: rank.getMinRank() */
int32_t getMinRank(CustomInstanceRankEnum rank);

/** Java: rank.getMinReward() */
int32_t getMinReward(CustomInstanceRankEnum rank);

/** Java: rank.getRankIcon() */
char16_t getRankIcon(CustomInstanceRankEnum rank);

/** Java: CustomInstanceRankEnum.getRankDescription(value). @throws runtime::IllegalArgumentException for a rank below 0 (getByRank) */
std::string getRankDescription(int32_t value);

/** Java: CustomInstanceRankEnum.getByRank(rank). @throws runtime::IllegalArgumentException "<rank> is no valid rank" for a rank below 0 */
CustomInstanceRankEnum getByRank(int32_t rank);

} // namespace aion::gameserver::custom::instance
