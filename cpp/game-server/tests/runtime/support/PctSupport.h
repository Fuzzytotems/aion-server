#pragma once

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"

// PCT helpers shared by all kernel tests (design §12.4). Usage:
//
//   TEST(MyPctTest, Scenario) {
//       AION_SKIP_WITHOUT_PCT();
//       pct::ScheduleResult result = pct::explore(testsupport::pctSchedules(200), 1, factory, check);
//       AION_EXPECT_SCHEDULE_OK(result);
//   }
//
// Kernel yield points exist only with AION_PCT (checked builds); in other builds PCT tests skip. Nightly runs raise the number of schedules
// with the AION_PCT_SCHEDULES environment variable.

namespace aion::gameserver::runtime::testsupport {

/** Not const, so the compiler cannot prove the code after AION_SKIP_WITHOUT_PCT unreachable in builds without PCT (C4702). */
inline bool pctCompiledIn = AION_PCT != 0;

/** Schedules for a PCT test: AION_PCT_SCHEDULES if set, else `defaultSchedules`. */
inline uint32_t pctSchedules(uint32_t defaultSchedules) {
	return pct::schedulesFromEnvironment(defaultSchedules);
}

/** Human-readable summary of a schedule (seed to replay, failure, trace tail). */
inline std::string describeSchedule(const pct::ScheduleResult& result) {
	std::string text = "seed " + std::to_string(result.seed) + ", steps " + std::to_string(result.steps) + (result.deadlock ? ", DEADLOCK" : "") +
		(result.failure.empty() ? "" : ", failure: " + result.failure);
	if (!result.trace.empty()) {
		text += "\ntrace (last 64):";
		size_t first = result.trace.size() > 64 ? result.trace.size() - 64 : 0;
		for (size_t i = first; i < result.trace.size(); ++i)
			text += "\n  " + result.trace[i];
	}
	return text;
}

/** true if the schedule failed because of the harness (script not satisfied, timeout) rather than a detected violation */
inline bool failedInHarness(const pct::ScheduleResult& result) {
	return result.deadlock || result.failure.find("PCT script") != std::string::npos;
}

/**
 * For death tests of mutated protocols: terminates the process with "LIFETIME VIOLATION" when the schedule detected a violation. Returns
 * normally if the schedule passed or failed in the harness (script not satisfied, timeout), so the death test then fails: a mutation must be
 * detected by the scenario itself, not by a broken script.
 */
inline void dieIfFailed(const pct::ScheduleResult& result) {
	if (!result.completed && !failedInHarness(result))
		checkFailed("LIFETIME VIOLATION", describeSchedule(result));
	if (!result.completed)
		std::fprintf(stderr, "schedule failed in the PCT harness, not by a detected violation: %s\n", describeSchedule(result).c_str());
}

} // namespace aion::gameserver::runtime::testsupport

#define AION_SKIP_WITHOUT_PCT()                                                                                                                     \
	if (!::aion::gameserver::runtime::testsupport::pctCompiledIn)                                                                                     \
	GTEST_SKIP() << "PCT yield points are compiled in checked builds only (AION_PCT)"

#define AION_EXPECT_SCHEDULE_OK(result) EXPECT_TRUE((result).completed) << ::aion::gameserver::runtime::testsupport::describeSchedule(result)
#define AION_ASSERT_SCHEDULE_OK(result) ASSERT_TRUE((result).completed) << ::aion::gameserver::runtime::testsupport::describeSchedule(result)
