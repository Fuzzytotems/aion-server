#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/quest/QuestRepeatCycle.h"

namespace aion::gameserver::model::templates::quest {

/**
 * Companion of the generated enum QuestRepeatCycle (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`getDay(cycle)` for Java `cycle.getDay()`). Java's enum implements L10n, which an enum class cannot
 * derive: `getL10nId(cycle)` and `getL10n(cycle)` stand for the interface methods.
 *
 * @author vlog
 */

namespace detail {
/** Java constructor arguments (weekDay, nameId) in ordinal order */
struct QuestRepeatCycleData {
	int32_t weekDay;
	int32_t nameId;
};

inline constexpr std::array<QuestRepeatCycleData, 8> QUEST_REPEAT_CYCLE_DATA{{
	{0, 0},      // ALL
	{1, 900331}, // MON
	{2, 900332}, // TUE
	{3, 900333}, // WED
	{4, 900334}, // THU
	{5, 900335}, // FRI
	{6, 900336}, // SAT
	{7, 900330}, // SUN
}};
static_assert(static_cast<size_t>(QuestRepeatCycle::SUN) + 1 == QUEST_REPEAT_CYCLE_DATA.size(), "one entry per QuestRepeatCycle constant");
} // namespace detail

constexpr int32_t getDay(QuestRepeatCycle cycle) noexcept {
	return detail::QUEST_REPEAT_CYCLE_DATA[static_cast<size_t>(cycle)].weekDay;
}

/** Java L10n.getL10nId() */
constexpr int32_t getL10nId(QuestRepeatCycle cycle) noexcept {
	return detail::QUEST_REPEAT_CYCLE_DATA[static_cast<size_t>(cycle)].nameId;
}

/** Java L10n.getL10n() (default method): ChatUtil.l10n(getL10nId()) */
std::string getL10n(QuestRepeatCycle cycle);

} // namespace aion::gameserver::model::templates::quest
