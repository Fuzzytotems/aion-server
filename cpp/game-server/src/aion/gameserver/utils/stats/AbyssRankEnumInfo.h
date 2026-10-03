#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::utils::stats {

/**
 * Companion of the generated enum AbyssRankEnum (docs/design/hub-headers.md §13, pattern XPRewardEnumInfo.h): Java's constructor data and its
 * accessors as free functions (`pointsGained(rank)` for Java `rank.getPointsGained()`, and so on). The stand-ins written before this companion
 * existed (AbyssRank.cpp's id/AP/GP table, ObjectsData.h's abyssRankId, AbyssRankDAO.cpp's getGpLossPerDay) keep their own copies of the same
 * values; the tests pin both to the Java table.
 *
 * @author ATracer, Sarynth, Imaginary
 */

namespace detail {
/** Java constructor data (id, pointsGained, pointsLost, requiredAP, requiredGP) in ordinal order (AbyssRankEnum.java:20-37) */
struct AbyssRankData {
	int32_t id;
	int32_t pointsGained;
	int32_t pointsLost;
	int32_t requiredAP;
	int32_t requiredGP;
};

inline constexpr std::array<AbyssRankData, 18> ABYSS_RANK_DATA{{
	{1, 300, 90, 0, 0},           // GRADE9_SOLDIER
	{2, 345, 103, 1200, 0},       // GRADE8_SOLDIER
	{3, 396, 118, 4220, 0},       // GRADE7_SOLDIER
	{4, 455, 136, 10990, 0},      // GRADE6_SOLDIER
	{5, 523, 156, 23500, 0},      // GRADE5_SOLDIER
	{6, 601, 180, 42780, 0},      // GRADE4_SOLDIER
	{7, 721, 216, 69700, 0},      // GRADE3_SOLDIER
	{8, 865, 259, 105600, 0},     // GRADE2_SOLDIER
	{9, 1038, 311, 150800, 0},    // GRADE1_SOLDIER
	{10, 1557, 467, 0, 1244},     // STAR1_OFFICER
	{11, 1868, 560, 0, 1368},     // STAR2_OFFICER
	{12, 2148, 644, 0, 1915},     // STAR3_OFFICER
	{13, 2470, 741, 0, 3064},     // STAR4_OFFICER
	{14, 3705, 1482, 0, 5210},    // STAR5_OFFICER
	{15, 4075, 1630, 0, 8335},    // GENERAL
	{16, 4482, 1792, 0, 10002},   // GREAT_GENERAL
	{17, 4930, 1972, 0, 11503},   // COMMANDER
	{18, 5916, 2366, 0, 12437},   // SUPREME_COMMANDER
}};
static_assert(static_cast<size_t>(AbyssRankEnum::SUPREME_COMMANDER) + 1 == ABYSS_RANK_DATA.size(), "one entry per AbyssRankEnum constant");

constexpr const AbyssRankData& abyssRankData(AbyssRankEnum rank) noexcept {
	return ABYSS_RANK_DATA[static_cast<size_t>(rank)];
}
} // namespace detail

/** Java: AbyssRankEnum.getPointsLost() */
constexpr int32_t pointsLost(AbyssRankEnum rank) noexcept {
	return detail::abyssRankData(rank).pointsLost;
}

/** Java: AbyssRankEnum.getPointsGained() */
constexpr int32_t pointsGained(AbyssRankEnum rank) noexcept {
	return detail::abyssRankData(rank).pointsGained;
}

/** Java: AbyssRankEnum.getRequiredAP() - AP required for Rank */
constexpr int32_t requiredAP(AbyssRankEnum rank) noexcept {
	return detail::abyssRankData(rank).requiredAP;
}

/** Java: AbyssRankEnum.getRequiredGP() */
constexpr int32_t requiredGP(AbyssRankEnum rank) noexcept {
	return detail::abyssRankData(rank).requiredGP;
}

/**
 * Java: AbyssRankEnum.getQuota() - RankingConfig.TOP_RANKING_QUOTA.getOrDefault(this, 0), the maximum number of players allowed to have the rank
 * (the config map is keyed by its placeholder enum, configs/detail/ConfigEnums.h, in the same ordinal order)
 */
inline int32_t quota(AbyssRankEnum rank) {
	auto quotas = configs::main::RankingConfig::TOP_RANKING_QUOTA.get();
	if (!quotas)
		return 0;
	auto it = quotas->find(static_cast<configs::detail::AbyssRankEnum>(static_cast<int32_t>(rank)));
	return it == quotas->end() ? 0 : it->second;
}

} // namespace aion::gameserver::utils::stats
