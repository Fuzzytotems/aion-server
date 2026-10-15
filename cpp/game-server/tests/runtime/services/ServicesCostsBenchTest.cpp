// Opt-in micro benchmark of the services (run with --gtest_also_run_disabled_tests --gtest_filter=*ServicesCostsBench*, preferably in Release).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using namespace std::chrono;

double nanosPerOp(steady_clock::time_point begin, steady_clock::time_point end, int64_t ops) {
	return static_cast<double>(duration_cast<nanoseconds>(end - begin).count()) / static_cast<double>(ops);
}

class ServicesCostsBenchTest : public DeterministicServicesTest {};

TEST_F(ServicesCostsBenchTest, DISABLED_Costs) {
	constexpr int32_t N = 1'000'000;
	IDFactory& factory = IDFactory::getInstance();
	IDFactory::Config config;
	config.releaseDelay = seconds(300);
	factory.configure(config);
	std::vector<int32_t> ids(N);
	auto t0 = steady_clock::now();
	for (int32_t i = 0; i < N; ++i)
		ids[static_cast<size_t>(i)] = factory.nextId();
	auto t1 = steady_clock::now();
	for (int32_t id : ids)
		factory.releaseId(id, "Npc");
	auto t2 = steady_clock::now();
	std::printf("IDFactory nextId %.1f ns, releaseId (quarantine) %.1f ns\n", nanosPerOp(t0, t1, N), nanosPerOp(t1, t2, N));

	CleanerQueue::setCleanerAction([](int32_t, const char*) {});
	auto t3 = steady_clock::now();
	for (int32_t i = 0; i < N; ++i)
		CleanerQueue::push(i);
	auto t4 = steady_clock::now();
	(void)CleanerQueue::drainNow();
	auto t5 = steady_clock::now();
	std::printf("CleanerQueue push %.1f ns, drain %.1f ns per id\n", nanosPerOp(t3, t4, N), nanosPerOp(t4, t5, N));

	LeakCensus::getInstance().install();
	Ref<TestObject> object = TestObject::create(1);
	auto t6 = steady_clock::now();
	for (int32_t i = 0; i < N; ++i) {
		LeakCensus::getInstance().onRemovedFromWorld(*object, "Npc", 1);
		LeakCensus::getInstance().onAddedToWorld(*object);
	}
	auto t7 = steady_clock::now();
	reclaim();
	auto t8 = steady_clock::now();
	std::printf("LeakCensus remove+add events %.1f ns, moved by a scan %.1f ns per event\n", nanosPerOp(t6, t7, N), nanosPerOp(t7, t8, 2 * int64_t{N}));

	services::cron::CronExpression weekly("0 0 17 ? * TUE,THU,SAT");
	const time_zone* utc = locate_zone("UTC");
	const time_zone* berlin = locate_zone("Europe/Berlin");
	constexpr int32_t M = 2000;
	sys_seconds after = sys_days(year(2024) / 1 / 1);
	auto t9 = steady_clock::now();
	for (int32_t i = 0; i < M; ++i)
		(void)weekly.getNextValidTimeAfter(after + seconds(i * 3600), utc);
	auto t10 = steady_clock::now();
	for (int32_t i = 0; i < M; ++i)
		(void)weekly.getNextValidTimeAfter(after + seconds(i * 3600), berlin);
	auto t11 = steady_clock::now();
	std::printf("CronExpression next fire time: UTC %.0f ns, Europe/Berlin %.0f ns\n", nanosPerOp(t9, t10, M), nanosPerOp(t10, t11, M));
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
