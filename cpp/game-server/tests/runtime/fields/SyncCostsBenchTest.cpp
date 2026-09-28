// Primitive costs of the sync and fields layer (design §2.5 estimates: Monitor ~20 ns, Field<float> load = mov, Field<Ref> load = TLS check +
// mov). Disabled by default; run with --gtest_also_run_disabled_tests --gtest_filter=*SyncCosts* in Release and in the checked build.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"

using namespace aion::gameserver::runtime;

namespace {

class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create() { return makeRef<Npc>(); }
	int32_t value = 1;

protected:
	Npc() = default;
	~Npc() override = default;
};

template <class F>
double nanosPerOperation(int64_t iterations, F&& body) {
	auto start = std::chrono::steady_clock::now();
	for (int64_t i = 0; i < iterations; ++i)
		body();
	auto elapsed = std::chrono::steady_clock::now() - start;
	return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) / static_cast<double>(iterations);
}

} // namespace

TEST(SyncCostsBenchTest, DISABLED_PrimitiveCosts) {
	constexpr int64_t N = 5'000'000;
	Monitor monitor{AION_LOCK_CLASS(Bench::monitor)};
	Monitor outer{AION_LOCK_CLASS(Bench::outer)};
	StampedLock stamped;
	Field<float> hp{1.0f};
	Field<int32_t> counter;
	AtomicInteger atomic;
	Ref<Npc> npc = Npc::create();
	Field<Ref<Npc>> target{npc};
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	volatile float sink = 0;
	volatile int32_t sinkInt = 0;

	std::printf("AION_CHECKED=%d\n", AION_CHECKED);
	std::printf("Monitor lock+unlock (uncontended):         %6.1f ns\n", nanosPerOperation(N, [&] {
		monitor.lock();
		monitor.unlock();
	}));
	std::printf("Monitor reentrant lock+unlock:             %6.1f ns\n", nanosPerOperation(N, [&] {
		SYNCHRONIZED(monitor) {
			SYNCHRONIZED(monitor) {
			}
		}
	}));
	outer.lock();
	std::printf("Monitor nested under another (lockdep):   %6.1f ns\n", nanosPerOperation(N, [&] {
		monitor.lock();
		monitor.unlock();
	}));
	outer.unlock();
	std::printf("StampedLock readLock+unlockRead:           %6.1f ns\n", nanosPerOperation(N, [&] { stamped.unlockRead(stamped.readLock()); }));
	std::printf("Field<float> load:                          %6.1f ns\n", nanosPerOperation(N, [&] { sink = hp.get(); }));
	std::printf("Field<int32_t> ++ (relaxed):                %6.1f ns\n", nanosPerOperation(N, [&] { ++counter; }));
	std::printf("AtomicInteger incrementAndGet:              %6.1f ns\n", nanosPerOperation(N, [&] { sinkInt = atomic.incrementAndGet(); }));
	std::printf("Field<Ref> load + deref:                    %6.1f ns\n", nanosPerOperation(N, [&] { sinkInt = target->value; }));
	(void)sink;
	(void)sinkInt;
}
