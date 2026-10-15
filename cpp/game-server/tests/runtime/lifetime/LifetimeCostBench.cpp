// Micro-benchmarks of the lifetime core (design §2.5 cost table). Disabled by default; run in the Release configuration with
//   aion_gs_runtime_lifetime_tests --gtest_also_run_disabled_tests --gtest_filter=*LifetimeCostBench*
// Single-threaded, uncontended numbers; they print ns per operation and assert nothing.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

#include "LifetimeTestSupport.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

namespace {

class Plain final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Plain> create() { return makeRef<Plain>(); }

protected:
	Plain() = default;
	~Plain() override = default;
};

template <class F>
void measure(const char* what, int iterations, F&& body) {
	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i)
		body();
	auto elapsed = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
	std::printf("%-58s %8.2f ns/op\n", what, elapsed / iterations);
}

} // namespace

TEST(LifetimeCostBench, DISABLED_PrimitiveCosts) {
	constexpr int N = 5'000'000;
	Ref<Plain> object = Plain::create();
	measure("Ref copy + non-last release (stamped once per epoch)", N, [&] { Ref<Plain> copy = object; });
	measure("TaskScope enter/exit (no publication)", N, [] { TaskScope scope(AION_TASK_INFO(TaskKind::TEST)); });
	measure("TaskScope enter + ensurePublished + exit", N, [] {
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		TaskScope::ensurePublished();
	});
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		TaskScope::ensurePublished();
		measure("ensurePublished (already published)", N, [] { TaskScope::ensurePublished(); });
		measure("Ptr from Ref + dereference", N, [&] {
			Ptr<Plain> borrowed = object;
			(void)borrowed->refCount();
		});
	}
	constexpr int M = 1'000'000;
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		measure("makeRef + last release (retire into thread list)", M, [] { (void)Plain::create(); });
	}
	auto start = std::chrono::steady_clock::now();
	Reclaimer::getInstance().drain(128);
	double drainNs = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
	std::printf("%-58s %8.2f ns/object\n", "scan + destruction", drainNs / M);
	std::printf("sizeof(RefCounted) = %zu, sizeof(Ptr) = %zu, sizeof(Ref) = %zu\n", sizeof(RefCounted), sizeof(Ptr<Plain>), sizeof(Ref<Plain>));
}
