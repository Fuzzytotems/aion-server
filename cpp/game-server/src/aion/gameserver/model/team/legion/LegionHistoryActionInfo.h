#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction_Type.h"

namespace aion::gameserver::model::team::legion {

/** Companion of the generated enum LegionHistoryAction (docs/design/static-data.md §2.5): Java's constructor data and methods (ADL). */

namespace detail {
/** Java: the constructor arguments (id as a byte, type), in ordinal order (LegionHistoryAction.java:8-23) */
struct LegionHistoryActionData {
	int8_t id;
	LegionHistoryAction_Type type;
};

inline constexpr std::array<LegionHistoryActionData, 15> LEGION_HISTORY_ACTION_DATA{{
	{0, LegionHistoryAction_Type::LEGION},     // CREATE
	{1, LegionHistoryAction_Type::LEGION},     // JOIN
	{2, LegionHistoryAction_Type::LEGION},     // KICK
	{3, LegionHistoryAction_Type::LEGION},     // LEVEL_UP
	{4, LegionHistoryAction_Type::LEGION},     // APPOINTED
	{5, LegionHistoryAction_Type::LEGION},     // EMBLEM_REGISTER
	{6, LegionHistoryAction_Type::LEGION},     // EMBLEM_MODIFIED
	{11, LegionHistoryAction_Type::REWARD},    // DEFENSE
	{12, LegionHistoryAction_Type::REWARD},    // OCCUPATION
	{13, LegionHistoryAction_Type::LEGION},    // LEGION_RENAME
	{14, LegionHistoryAction_Type::LEGION},    // CHARACTER_RENAME
	{15, LegionHistoryAction_Type::WAREHOUSE}, // ITEM_DEPOSIT
	{16, LegionHistoryAction_Type::WAREHOUSE}, // ITEM_WITHDRAW
	{17, LegionHistoryAction_Type::WAREHOUSE}, // KINAH_DEPOSIT
	{18, LegionHistoryAction_Type::WAREHOUSE}, // KINAH_WITHDRAW
}};
static_assert(static_cast<size_t>(LegionHistoryAction::KINAH_WITHDRAW) + 1 == LEGION_HISTORY_ACTION_DATA.size(),
	"one entry per LegionHistoryAction constant");
} // namespace detail

/** Java: LegionHistoryAction.getId() */
constexpr int8_t getId(LegionHistoryAction action) noexcept {
	return detail::LEGION_HISTORY_ACTION_DATA[static_cast<size_t>(action)].id;
}

/** Java: LegionHistoryAction.getType() */
constexpr LegionHistoryAction_Type getType(LegionHistoryAction action) noexcept {
	return detail::LEGION_HISTORY_ACTION_DATA[static_cast<size_t>(action)].type;
}

} // namespace aion::gameserver::model::team::legion
