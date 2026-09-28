// Kernel stress harness v0 entry points (design §12.4, §19 P2).
//
// - KernelStressTest.Smoke runs in every test run: a few seconds with a reduced worker count, all pass criteria enforced.
// - KernelStressTest.LongRun is opt-in: set AION_STRESS_SECONDS (e.g. 600) and optionally AION_STRESS_THREADS, AION_STRESS_SEED,
//   AION_STRESS_FAULTS, AION_STRESS_REPORT, AION_STRESS_VERBOSE=1. Without AION_STRESS_SECONDS it is skipped.
//   Example: AION_STRESS_SECONDS=600 AION_STRESS_VERBOSE=1 aion_gs_runtime_stress_tests --gtest_filter=KernelStressTest.LongRun

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <thread>

#include "StressHarness.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"

using namespace aion::gameserver::runtime::stress;
using namespace std::chrono_literals;

namespace {

std::string describe(const StressResult& result) {
	std::string text = result.report;
	for (const std::string& failure : result.failures)
		text += "\nFAILURE: " + failure;
	return text;
}

} // namespace

TEST(KernelStressTest, Smoke) {
	StressConfig defaults;
	defaults.duration = 4s;
	defaults.workerThreads = static_cast<int32_t>(std::clamp(std::thread::hardware_concurrency(), 2u, 8u));
	defaults.faultPermille = 20;
	StressConfig config = StressConfig::fromEnvironment(defaults);
	config.duration = defaults.duration; // the smoke run never takes the long-run duration
	config.workerThreads = defaults.workerThreads;
	StressResult result = runKernelStress(config);
	EXPECT_TRUE(result.passed) << describe(result);
}

TEST(KernelStressTest, LongRun) {
	if (StressConfig::requestedLongRunDuration().count() <= 0) {
		GTEST_SKIP() << "set AION_STRESS_SECONDS to run the long kernel stress run";
	}
	StressConfig config = StressConfig::fromEnvironment(StressConfig{});
	StressResult result = runKernelStress(config);
	EXPECT_TRUE(result.passed) << describe(result);
}

// Harness self-test (opt-in, checked builds): with one lifetime protocol step switched off (detail::Mutation, the same switches the lifetime
// mutation tests use) a short stress run must detect the broken protocol - a canary/ASan use-after-free report, a C4/C5 termination or a
// failed end check (leak, backlog). Each mutation runs in a death-test child; exit code 0 means undetected, SUPERVISOR_EXIT_CODE a hang.
// Run: AION_STRESS_SELFTEST=1 aion_gs_runtime_stress_tests --gtest_filter=KernelStressSelfTest.*

namespace {

using aion::gameserver::runtime::detail::Mutation;

struct NamedMutation {
	Mutation mutation;
	const char* name;
};

constexpr NamedMutation SELF_TEST_MUTATIONS[] = {
	{Mutation::SKIP_RELEASE_STAMP, "SKIP_RELEASE_STAMP"},
	{Mutation::CAS_BEFORE_STAMP, "CAS_BEFORE_STAMP"},
	{Mutation::DESIGN_STAMPLESS_FAST_PATH, "DESIGN_STAMPLESS_FAST_PATH"},
	{Mutation::STAMP_OVERWRITE, "STAMP_OVERWRITE"},
	{Mutation::NON_ATOMIC_DECREMENT, "NON_ATOMIC_DECREMENT"},
	{Mutation::RELEASE_ALWAYS_PUSH, "RELEASE_ALWAYS_PUSH"},
	{Mutation::SCAN_NO_ADVANCE, "SCAN_NO_ADVANCE"},
	{Mutation::SCAN_IGNORE_PUBLISHED, "SCAN_IGNORE_PUBLISHED"},
	{Mutation::SCAN_MIN_AFTER_OBJECTS, "SCAN_MIN_AFTER_OBJECTS"},
	{Mutation::SCAN_KEEP_WITHOUT_CLEAR, "SCAN_KEEP_WITHOUT_CLEAR"},
	{Mutation::SCAN_DROP_WITHOUT_RECHECK, "SCAN_DROP_WITHOUT_RECHECK"},
	{Mutation::SCAN_REQUEUE_WITHOUT_EXCHANGE, "SCAN_REQUEUE_WITHOUT_EXCHANGE"},
	{Mutation::PART_STAMP_ZERO, "PART_STAMP_ZERO"},
	{Mutation::NODE_STAMP_ZERO, "NODE_STAMP_ZERO"},
	{Mutation::PUBLISH_NOTHING, "PUBLISH_NOTHING"},
	{Mutation::QUIESCENT_IGNORE_RULES, "QUIESCENT_IGNORE_RULES"},
	{Mutation::SCOPE_EXIT_NO_UNPUBLISH, "SCOPE_EXIT_NO_UNPUBLISH"},
};

/** gtest/ctest display of the parameter */
void PrintTo(const NamedMutation& mutation, std::ostream* out) {
	*out << mutation.name;
}

class KernelStressSelfTest : public testing::TestWithParam<NamedMutation> {};

bool detected(int exitCode) {
	return exitCode != 0 && exitCode != SUPERVISOR_EXIT_CODE;
}

} // namespace

TEST_P(KernelStressSelfTest, MutationIsDetected) {
	const char* enabled = std::getenv("AION_STRESS_SELFTEST"); // test-only switch
	if (enabled == nullptr || *enabled != '1') {
		GTEST_SKIP() << "set AION_STRESS_SELFTEST=1 to run the harness self-test";
	}
	if (!aion::gameserver::runtime::CHECKED || AION_LIFETIME_MUTATIONS == 0) {
		GTEST_SKIP() << "mutations are compiled in checked builds only";
	}
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	StressConfig config = StressConfig::fromEnvironment(StressConfig{});
	config.duration = 8s;
	config.workerThreads = 16;
	config.teardownTimeout = 40s;
	config.mutation = static_cast<int32_t>(GetParam().mutation);
	EXPECT_EXIT(
		{
			StressResult result = runKernelStress(config);
			std::fflush(stdout);
			std::_Exit(result.passed ? 0 : 2);
		},
		detected, "")
		<< "mutation " << GetParam().name << " was not detected by a stress run";
}

INSTANTIATE_TEST_SUITE_P(Mutations, KernelStressSelfTest, testing::ValuesIn(SELF_TEST_MUTATIONS),
	[](const testing::TestParamInfo<NamedMutation>& info) { return std::string(info.param.name); });
