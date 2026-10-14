#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/instance/StageType.h"

namespace aion::gameserver::model::instance {

/**
 * Companion of the generated enum StageType (static-data.md §2.5, InstanceProgressionTypeInfo.h's pattern): Java's constructor data and
 * accessors as free functions found by ADL (`getId(value)` for Java `value.getId()`).
 */

namespace detail {
/** Java constructor arguments (id, type) in ordinal order */
struct StageTypeData {
	int32_t id;
	int32_t type;
};
inline constexpr std::array<StageTypeData, 88> STAGETYPE_DATA{{
	{0, 0}, // DEFAULT
	{35464, 1}, // START_STAGE_1_ELEVATOR
	{35465, 1}, // START_STAGE_1_ROUND_1
	{35466, 1}, // START_STAGE_1_ROUND_2
	{35467, 1}, // START_STAGE_1_ROUND_3
	{35468, 1}, // START_STAGE_1_ROUND_4
	{35469, 1}, // START_STAGE_1_ROUND_5
	{36464, 1}, // START_STAGE_2_ELEVATOR
	{36465, 1}, // START_STAGE_2_ROUND_1
	{36466, 1}, // START_STAGE_2_ROUND_2
	{36467, 1}, // START_STAGE_2_ROUND_3
	{36468, 1}, // START_STAGE_2_ROUND_4
	{36469, 1}, // START_STAGE_2_ROUND_5
	{37464, 1}, // START_STAGE_3_ELEVATOR
	{37465, 1}, // START_STAGE_3_ROUND_1
	{37466, 1}, // START_STAGE_3_ROUND_2
	{37467, 1}, // START_STAGE_3_ROUND_3
	{37468, 1}, // START_STAGE_3_ROUND_4
	{37469, 1}, // START_STAGE_3_ROUND_5
	{38464, 1}, // START_STAGE_4_ELEVATOR
	{38465, 1}, // START_STAGE_4_ROUND_1
	{38465, 1}, // START_ALTERNATIVE_STAGE_4_ROUND_1
	{38466, 1}, // START_STAGE_4_ROUND_2
	{38467, 1}, // START_STAGE_4_ROUND_3
	{38468, 1}, // START_STAGE_4_ROUND_4
	{38469, 1}, // START_STAGE_4_ROUND_5
	{8392, 3}, // START_STAGE_5
	{8393, 3}, // START_STAGE_5_ROUND_1
	{8394, 3}, // START_STAGE_5_ROUND_2
	{8395, 3}, // START_STAGE_5_ROUND_3
	{8396, 3}, // START_STAGE_5_ROUND_4
	{8397, 3}, // START_STAGE_5_ROUND_5
	{43856, 4}, // START_STAGE_6
	{43857, 4}, // START_STAGE_6_ROUND_1
	{43858, 4}, // START_STAGE_6_ROUND_2
	{43859, 4}, // START_STAGE_6_ROUND_3
	{43860, 4}, // START_STAGE_6_ROUND_4
	{43861, 4}, // START_STAGE_6_ROUND_5
	{13784, 6}, // START_STAGE_7
	{13785, 6}, // START_STAGE_7_ROUND_1
	{13786, 6}, // START_STAGE_7_ROUND_2
	{13787, 6}, // START_STAGE_7_ROUND_3
	{13788, 6}, // START_STAGE_7_ROUND_4
	{13789, 6}, // START_STAGE_7_ROUND_5
	{49248, 7}, // START_STAGE_8
	{49249, 7}, // START_STAGE_8_ROUND_1
	{49250, 7}, // START_STAGE_8_ROUND_2
	{49251, 7}, // START_STAGE_8_ROUND_3
	{49252, 7}, // START_STAGE_8_ROUND_4
	{49253, 7}, // START_STAGE_8_ROUND_5
	{19176, 9}, // START_STAGE_9
	{19177, 9}, // START_STAGE_9_ROUND_1
	{19178, 9}, // START_STAGE_9_ROUND_2
	{19179, 9}, // START_STAGE_9_ROUND_3
	{19180, 9}, // START_STAGE_9_ROUND_4
	{19181, 9}, // START_STAGE_9_ROUND_5
	{54640, 10}, // START_STAGE_10
	{54641, 10}, // START_STAGE_10_ROUND_1
	{54642, 10}, // START_STAGE_10_ROUND_2
	{54643, 10}, // START_STAGE_10_ROUND_3
	{54644, 10}, // START_STAGE_10_ROUND_4
	{54645, 10}, // START_STAGE_10_ROUND_5
	{35566, 1}, // PASS_STAGE_1
	{36565, 1}, // PASS_STAGE_2
	{38566, 1}, // PASS_STAGE_4
	{39566, 1}, // PASS_STAGE_5
	{40565, 1}, // PASS_STAGE_6
	{35569, 1}, // PASS_GROUP_STAGE_1
	{36569, 1}, // PASS_GROUP_STAGE_2
	{37569, 1}, // PASS_GROUP_STAGE_3
	{38569, 1}, // PASS_GROUP_STAGE_4
	{8497, 3}, // PASS_GROUP_STAGE_5
	{43961, 4}, // PASS_GROUP_STAGE_6
	{13789, 6}, // PASS_GROUP_STAGE_7
	{49253, 7}, // PASS_GROUP_STAGE_8
	{19181, 9}, // PASS_GROUP_STAGE_9
	{54645, 10}, // PASS_GROUP_STAGE_10
	{36470, 1}, // START_BONUS_STAGE_2
	{37470, 1}, // START_BONUS_STAGE_3
	{38470, 1}, // START_BONUS_STAGE_4
	{43862, 4}, // START_BONUS_STAGE_6
	{1, 0}, // PVP_STAGE_1
	{2, 0}, // PVP_STAGE_2
	{3, 0}, // PVP_STAGE_3
	{4, 0}, // PVP_STAGE_4
	{5, 0}, // PVP_STAGE_5
	{6, 0}, // PVP_STAGE_6
	{0, 0}, // PVP_STAGE_OVER
}};
static_assert(static_cast<size_t>(StageType::PVP_STAGE_OVER) + 1 == STAGETYPE_DATA.size(), "one entry per constant");
} // namespace detail

/** Java: getId() */
constexpr int32_t getId(StageType value) noexcept {
	return detail::STAGETYPE_DATA[static_cast<size_t>(value)].id;
}

/** Java: getType() */
constexpr int32_t getType(StageType value) noexcept {
	return detail::STAGETYPE_DATA[static_cast<size_t>(value)].type;
}

} // namespace aion::gameserver::model::instance
