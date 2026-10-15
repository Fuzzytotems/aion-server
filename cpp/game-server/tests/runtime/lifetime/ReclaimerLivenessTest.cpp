// Liveness of the Reclaimer while a task pins reclamation (30-minute ASan stress finding, 2026-09-13): one task publishes an epoch and blocks
// for seconds while other threads keep retiring objects and nodes at a high rate. The Reclaimer must keep advancing the global epoch at its
// period, a scan must not re-walk the part of the backlog that the pinned epoch keeps alive, and once the task ends the backlog must drain in
// bounded time while the epoch keeps advancing.
//
// PinnedPublisherDoesNotStallReclamation runs by default (short pin). The long variant is opt-in (--gtest_also_run_disabled_tests); both read
// AION_LIVENESS_PIN_MS, AION_LIVENESS_RATE (retires per second, all threads) and AION_LIVENESS_THREADS.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "LifetimeTestSupport.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;
using namespace std::chrono_literals;

namespace {

using Clock = std::chrono::steady_clock;

class ChurnObject final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static constexpr uint64_t MAGIC = 0xC4A7'0B1E'C7ED'0001;
	static Ref<ChurnObject> create(uint64_t value) { return makeRef<ChurnObject>(value); }
	uint64_t value() const noexcept { return value_; }

protected:
	explicit ChurnObject(uint64_t value) : value_(MAGIC ^ value) {}
	~ChurnObject() override { value_ = 0; }

private:
	uint64_t value_;
	std::array<uint64_t, 3> payload_{};
};

class ChurnNode final : public RetiredNode {
public:
	size_t retiredBytes() const noexcept override { return sizeof(ChurnNode); }

private:
	std::array<uint64_t, 4> payload_{};
};

int64_t envOr(const char* name, int64_t fallback) {
	if (const char* value = std::getenv(name); value != nullptr && *value != '\0')
		return std::strtoll(value, nullptr, 10);
	return fallback;
}

struct Params {
	std::chrono::milliseconds pin;
	int64_t retiresPerSecond;
	int32_t threads;
	std::chrono::milliseconds period{20};
};

Params paramsFromEnvironment(Params defaults) {
	defaults.pin = std::chrono::milliseconds(envOr("AION_LIVENESS_PIN_MS", defaults.pin.count()));
	defaults.retiresPerSecond = envOr("AION_LIVENESS_RATE", defaults.retiresPerSecond);
	defaults.threads = static_cast<int32_t>(envOr("AION_LIVENESS_THREADS", defaults.threads));
	return defaults;
}

struct Sample {
	Clock::time_point time;
	Reclaimer::Stats stats;
};

/** Longest time the epoch stayed unchanged within [from, to] (a change is attributed to the first sample showing it). */
std::chrono::milliseconds maxEpochGap(const std::vector<Sample>& samples, Clock::time_point from, Clock::time_point to) {
	std::chrono::milliseconds longest{0};
	Clock::time_point lastChange = from;
	uint64_t lastEpoch = 0;
	bool first = true;
	for (const Sample& sample : samples) {
		if (sample.time < from || sample.time > to)
			continue;
		if (first || sample.stats.epoch != lastEpoch) {
			if (!first)
				longest = std::max(longest, std::chrono::duration_cast<std::chrono::milliseconds>(sample.time - lastChange));
			lastChange = sample.time;
			lastEpoch = sample.stats.epoch;
			first = false;
		}
	}
	longest = std::max(longest, std::chrono::duration_cast<std::chrono::milliseconds>(to - lastChange));
	return longest;
}

const Sample& sampleAt(const std::vector<Sample>& samples, Clock::time_point time) {
	for (const Sample& sample : samples)
		if (sample.time >= time)
			return sample;
	return samples.back();
}

double millisBetween(Clock::time_point from, Clock::time_point to) {
	return std::chrono::duration<double, std::milli>(to - from).count();
}

void runPinnedPublisherScenario(const Params& params) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	const Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = params.period;
	config.backlogDumpObjects = SIZE_MAX; // no watchdog dumps or lag warnings: this test measures
	config.backlogDumpBytes = SIZE_MAX;
	config.lagWarning = std::chrono::hours(24);
	config.scanTimeBudget = std::chrono::microseconds(envOr("AION_LIVENESS_SCAN_BUDGET_US", config.scanTimeBudget.count())); // 0: unbounded
	reclaimer.start(config);

	SharedLocation<ChurnObject> location;
	location.store(ChurnObject::create(0));
	std::atomic<bool> stopChurn{false};
	std::atomic<uint64_t> retired{0};
	std::atomic<uint64_t> violations{0};
	const double perThreadRate = static_cast<double>(params.retiresPerSecond) / params.threads;
	std::vector<std::thread> churners;
	for (int32_t t = 0; t < params.threads; ++t) {
		churners.emplace_back([&, t] {
			const Clock::time_point start = Clock::now();
			uint64_t done = 0;
			uint64_t sequence = static_cast<uint64_t>(t) << 40;
			while (!stopChurn.load(std::memory_order_relaxed)) {
				{
					TaskScope scope(testTask());
					for (int i = 0; i < 16; ++i) {
						Ref<ChurnObject> object = ChurnObject::create(++sequence);
						Ptr<ChurnObject> borrowed = object; // publishes, like a game task
						if ((sequence & 7) == 0)
							location.store(object); // the previous shared object is released (and retired) after its unlink
						object.reset();             // last release unless stored: retired to this scope's retire list
						if (borrowed->value() != (ChurnObject::MAGIC ^ sequence))
							violations.fetch_add(1);
						Reclaimer::retireNode(std::make_unique<ChurnNode>());
					}
				}
				done += 32;
				retired.fetch_add(32, std::memory_order_relaxed);
				double allowed = perThreadRate * std::chrono::duration<double>(Clock::now() - start).count();
				if (static_cast<double>(done) > allowed)
					std::this_thread::sleep_for(1ms);
			}
		});
	}

	std::vector<Sample> samples;
	samples.reserve(200'000);
	auto sampleFor = [&](std::chrono::milliseconds duration, const std::function<bool()>& until = nullptr) {
		const Clock::time_point end = Clock::now() + duration;
		while (Clock::now() < end) {
			samples.push_back(Sample{Clock::now(), reclaimer.stats()});
			if (until && until())
				return true;
			std::this_thread::sleep_for(1ms);
		}
		return false;
	};

	// warm-up: steady state without a pin
	sampleFor(1000ms);
	const Clock::time_point steadyFrom = samples.front().time + 300ms;
	uint64_t steadyBacklogMax = 0;
	for (const Sample& sample : samples)
		if (sample.time >= steadyFrom)
			steadyBacklogMax = std::max(steadyBacklogMax, sample.stats.backlog);

	// the pinned publisher: borrows the shared object (which the churners replace and release meanwhile) and blocks
	std::atomic<bool> pinned{false};
	std::atomic<bool> releasing{false};
	std::atomic<uint64_t> pinnedEpoch{0};
	std::thread publisher([&] {
		TaskScope scope(testTask());
		Ptr<ChurnObject> borrowed = location.load();
		pinnedEpoch = ThreadContext::current().publishedEpoch.load();
		uint64_t before = borrowed->value();
		pinned = true;
		std::this_thread::sleep_for(params.pin); // a slow DB call holding borrows
		if (borrowed->value() != before)
			violations.fetch_add(1);
		releasing = true;
	});
	while (!pinned.load())
		std::this_thread::sleep_for(100us);
	const Clock::time_point pinStart = Clock::now();
	sampleFor(params.pin + 5s, [&] { return releasing.load(); });
	publisher.join();
	const Clock::time_point pinEnd = Clock::now();

	// after the release, with the churn still running: the backlog must return to its steady-state size
	const uint64_t recoveredBacklog = std::max<uint64_t>(steadyBacklogMax * 2, 20'000);
	const auto drainBound = std::max<std::chrono::milliseconds>(3000ms, params.pin);
	const bool recovered = sampleFor(drainBound + 10s, [&] { return samples.back().stats.backlog <= recoveredBacklog; });
	const Clock::time_point recoveredAt = samples.back().time;
	sampleFor(500ms); // the epoch keeps advancing with the churn after recovery

	stopChurn = true;
	for (std::thread& thread : churners)
		thread.join();
	const Clock::time_point runEnd = Clock::now();
	reclaimer.stop();
	reclaimer.configure(original);
	location.store(nullptr);
	drainReclaimer();

	// measurements
	const Sample& atPinStart = sampleAt(samples, pinStart);
	const Sample& atPinEnd = sampleAt(samples, pinEnd);
	const Sample& atRecovered = sampleAt(samples, recoveredAt);
	uint64_t pinnedBacklogMax = 0;
	uint64_t pinnedMinActiveSamples = 0;
	uint64_t windowSamples = 0;
	for (const Sample& sample : samples) {
		if (sample.time >= pinStart && sample.time <= pinEnd) {
			pinnedBacklogMax = std::max(pinnedBacklogMax, sample.stats.backlog);
			++windowSamples;
			if (sample.stats.minActive <= pinnedEpoch.load())
				++pinnedMinActiveSamples;
		}
	}
	const Clock::time_point measuredFrom = pinStart + 200ms; // the scan that was running when the pin started may be long
	const auto pinnedGap = maxEpochGap(samples, measuredFrom, pinEnd);
	const auto drainGap = maxEpochGap(samples, pinEnd, recoveredAt);
	const auto runGap = maxEpochGap(samples, samples.front().time, runEnd);
	const double pinMillis = millisBetween(pinStart, pinEnd);
	const double pinnedScanRate = (atPinEnd.stats.scans - atPinStart.stats.scans) * 1000.0 / pinMillis;
	const double drainMillis = millisBetween(pinEnd, recoveredAt);
	const uint64_t pinnedExamined = atPinEnd.stats.examinedTotal - atPinStart.stats.examinedTotal;
	const uint64_t pinnedScans = atPinEnd.stats.scans - atPinStart.stats.scans;
	uint64_t maxScanMicros = 0;
	for (const Sample& sample : samples)
		if (sample.time >= measuredFrom && sample.time <= recoveredAt)
			maxScanMicros = std::max<uint64_t>(maxScanMicros, sample.stats.lastScanDuration.count());
	std::printf("[liveness] while pinned: %llu scans examined %llu entries (backlog at pin start %llu); longest sampled scan %.1f ms; bounded scans %llu\n",
		static_cast<unsigned long long>(pinnedScans), static_cast<unsigned long long>(pinnedExamined),
		static_cast<unsigned long long>(atPinStart.stats.backlog), maxScanMicros / 1000.0,
		static_cast<unsigned long long>(samples.back().stats.budgetExhaustedScans - samples.front().stats.budgetExhaustedScans));
	std::printf("[liveness] pin %.0f ms, rate %lld/s, %d threads, retired %llu; steady backlog max %llu; pinned backlog max %llu (%.1f MB); "
				"scans/s while pinned %.1f (nominal %.1f); max epoch gap pinned %lld ms, draining %lld ms, whole run %lld ms; drain to %llu entries "
				"%.0f ms (%s); destroyed while draining %llu\n",
		pinMillis, static_cast<long long>(params.retiresPerSecond), params.threads, static_cast<unsigned long long>(retired.load()),
		static_cast<unsigned long long>(steadyBacklogMax), static_cast<unsigned long long>(pinnedBacklogMax),
		atPinEnd.stats.backlogBytes / (1024.0 * 1024.0), pinnedScanRate, 1000.0 / params.period.count(), static_cast<long long>(pinnedGap.count()),
		static_cast<long long>(drainGap.count()), static_cast<long long>(runGap.count()), static_cast<unsigned long long>(recoveredBacklog), drainMillis,
		recovered ? "recovered" : "NOT recovered", static_cast<unsigned long long>(atRecovered.stats.destroyedTotal - atPinEnd.stats.destroyedTotal));
	std::fflush(stdout);

	EXPECT_EQ(violations.load(), 0u) << "a borrowed object was destroyed or corrupted";
	// sanity: the scenario pinned reclamation and built a backlog
	EXPECT_GE(pinnedMinActiveSamples * 10, windowSamples * 9) << "the publisher did not hold the oldest epoch while pinned";
	EXPECT_GE(pinnedBacklogMax, static_cast<uint64_t>(params.retiresPerSecond * pinMillis / 1000.0 * 0.25)) << "the churn did not build a backlog";
	// liveness
	const auto gapBound = std::max<std::chrono::milliseconds>(500ms, params.period * 25);
	EXPECT_LE(pinnedGap, gapBound) << "the global epoch stalled while the publisher was pinned";
	EXPECT_LE(drainGap, gapBound) << "the global epoch stalled while the backlog drained";
	EXPECT_LE(runGap, gapBound) << "the global epoch stalled";
	EXPECT_GE(pinnedScanRate, 0.3 * 1000.0 / params.period.count()) << "scans slowed down while the backlog grew";
	// work per scan: entries retired after the publication cannot become eligible while it is pinned, so the scans of the whole pinned window
	// examine at most what was queued (or in flight in the churners' retire lists) when the pin started
	const uint64_t examinedBound = atPinStart.stats.backlog + 4096 + static_cast<uint64_t>(params.threads) * 256;
	EXPECT_LE(pinnedExamined, examinedBound) << "scans re-examined the part of the backlog that the pinned epoch keeps alive";
	EXPECT_TRUE(recovered) << "the backlog did not return to " << recoveredBacklog << " entries";
	EXPECT_LE(drainMillis, static_cast<double>(drainBound.count())) << "the backlog drained too slowly";
}

/** A thread that publishes an epoch and holds it until released. */
class PinningThread {
public:
	PinningThread()
		: thread_([this] {
			  TaskScope scope(testTask());
			  TaskScope::ensurePublished();
			  epoch_ = ThreadContext::current().publishedEpoch.load();
			  published_ = true;
			  while (!release_.load())
				  std::this_thread::sleep_for(1ms);
		  }) {
		while (!published_.load())
			std::this_thread::sleep_for(1ms);
	}
	~PinningThread() { release(); }
	PinningThread(const PinningThread&) = delete;
	PinningThread& operator=(const PinningThread&) = delete;

	uint64_t epoch() const { return epoch_.load(); }
	void release() {
		release_ = true;
		if (thread_.joinable())
			thread_.join();
	}

private:
	std::atomic<bool> published_{false};
	std::atomic<bool> release_{false};
	std::atomic<uint64_t> epoch_{0};
	std::thread thread_;
};

} // namespace

struct PinnedScanCounts {
	uint64_t backlog = 0;
	/** entries examined by the 20 scans while pinned */
	uint64_t examinedWhilePinned = 0;
	uint64_t epochsAdvanced = 0;
	uint64_t destroyedWhilePinned = 0;
	uint64_t examinedAfterRelease = 0;
	uint64_t destroyedAfterRelease = 0;
	uint64_t backlogAfterRelease = 0;
};

/** Retires objects and nodes after a task published its epoch, runs 20 scans, releases the task and runs one more scan. */
PinnedScanCounts scanWhilePinned(uint64_t perKind) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	PinnedScanCounts counts;
	auto pin = std::make_unique<PinningThread>();
	{
		TaskScope scope(testTask()); // batched retire lists, flushed at scope exit; nothing is loaded, so this thread does not publish
		for (uint64_t i = 0; i < perKind; ++i) {
			ChurnObject::create(i).reset();
			Reclaimer::retireNode(std::make_unique<ChurnNode>());
		}
	}
	const Reclaimer::Stats before = reclaimer.stats();
	counts.backlog = before.backlog;
	for (int scan = 0; scan < 20; ++scan) {
		reclaimer.reclaimNow();
		EXPECT_EQ(reclaimer.stats().minActive, pin->epoch());
	}
	const Reclaimer::Stats pinned = reclaimer.stats();
	counts.examinedWhilePinned = pinned.examinedTotal - before.examinedTotal;
	counts.epochsAdvanced = pinned.epoch - before.epoch;
	counts.destroyedWhilePinned = pinned.destroyedTotal - before.destroyedTotal;
	pin->release();
	reclaimer.reclaimNow();
	const Reclaimer::Stats released = reclaimer.stats();
	counts.examinedAfterRelease = released.lastScanExamined;
	counts.destroyedAfterRelease = released.lastScanDestroyed;
	counts.backlogAfterRelease = released.backlog;
	return counts;
}

// Entries retired after a task published its epoch cannot be destroyed while it stays published; scans must not examine them again and again.
TEST(ReclaimerLivenessTest, ScansDoNotReexamineEntriesThatAPinnedEpochKeepsAlive) {
	ASSERT_FALSE(Reclaimer::getInstance().isRunning());
	drainReclaimer();
	constexpr uint64_t PER_KIND = 20'000;
	PinnedScanCounts counts = scanWhilePinned(PER_KIND);
	EXPECT_EQ(counts.backlog, 2 * PER_KIND);
	EXPECT_EQ(counts.examinedWhilePinned, 0u) << "scans examined entries retired after the pinned epoch";
	EXPECT_EQ(counts.destroyedWhilePinned, 0u);
	EXPECT_EQ(counts.epochsAdvanced, 20u) << "every scan advances the epoch";
	EXPECT_EQ(counts.examinedAfterRelease, 2 * PER_KIND) << "work proportional to what became eligible";
	EXPECT_EQ(counts.destroyedAfterRelease, 2 * PER_KIND);
	EXPECT_EQ(counts.backlogAfterRelease, 0u);
}

// The test above detects a scan that walks the whole backlog (the scan before the limbo), with the same scenario.
TEST(ReclaimerLivenessTest, WholeBacklogWalkIsDetected) {
	if (AION_LIFETIME_MUTATIONS == 0)
		GTEST_SKIP() << "mutations are compiled in checked builds only";
	ASSERT_FALSE(Reclaimer::getInstance().isRunning());
	drainReclaimer();
	constexpr uint64_t PER_KIND = 20'000;
	PinnedScanCounts counts;
	{
		detail::MutationScope mutation(detail::Mutation::SCAN_EXAMINE_WHOLE_LIMBO);
		counts = scanWhilePinned(PER_KIND);
	}
	EXPECT_EQ(counts.examinedWhilePinned, 20 * 2 * PER_KIND) << "every pinned scan walks the whole backlog";
	EXPECT_EQ(counts.destroyedWhilePinned, 0u);
	EXPECT_EQ(counts.backlogAfterRelease, 0u);
}

// An entry found with a newer stamp (a resurrected object released again, a retired part released through a Ref) is filed again under that
// stamp, so it is examined once more only after the pinned epoch has moved past it.
TEST(ReclaimerLivenessTest, RestampedEntriesAreRebucketedUnderTheirCurrentStamp) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();

	// an object queued with stamp e0 and resurrected; the part retired at e0 and held by a Ref
	SharedLocation<Tracked> location;
	location.store(Tracked::create(tracker));
	Ref<PartOwner> owner = PartOwner::create();
	owner->map.put(1, std::make_unique<TrackedPart>(*owner, tracker));
	Ref<Tracked> resurrected;
	Ref<TrackedPart> heldPart;
	{
		TaskScope scope(testTask());
		Ptr<Tracked> borrowed = location.load();
		location.store(nullptr); // last release: stamped, queued
		resurrected = Ref<Tracked>(borrowed);
		heldPart = Ref<TrackedPart>(owner->map.get(1));
	}
	owner->map.put(1, std::make_unique<TrackedPart>(*owner, tracker)); // retires the held part (outside scopes: pushed at once)
	reclaimer.reclaimNow();
	EXPECT_EQ(reclaimer.stats().lastScanExamined, 2u) << "the resurrected object (dropped) and the held part (kept)";
	EXPECT_EQ(reclaimer.stats().backlog, 1u);
	reclaimer.reclaimNow();
	EXPECT_EQ(reclaimer.stats().lastScanExamined, 1u) << "a held part is examined by every scan whose m passed its stamp";

	// queue the object again (key e1) and resurrect it; then a task pins a newer epoch and both entries are released with newer stamps
	{
		TaskScope scope(testTask());
		location.store(std::move(resurrected));
		Ptr<Tracked> borrowed = location.load();
		location.store(nullptr);
		resurrected = Ref<Tracked>(borrowed);
	}
	detail::testing::advanceEpoch();
	auto pin = std::make_unique<PinningThread>();
	resurrected.reset(); // count 0 with stamp E >= pinned epoch; `queued` is still set, so nothing is pushed
	heldPart.reset();    // stamps the part with E >= pinned epoch
	reclaimer.reclaimNow();
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_EQ(stats.lastScanExamined, 2u) << "both entries had keys below the pinned epoch";
	EXPECT_EQ(stats.lastScanDestroyed, 0u);
	EXPECT_EQ(stats.backlog, 2u);
	for (int scan = 0; scan < 5; ++scan) {
		reclaimer.reclaimNow();
		EXPECT_EQ(reclaimer.stats().lastScanExamined, 0u) << "re-bucketed under stamps the pinned epoch keeps alive";
	}
	EXPECT_EQ(tracker->destroyedCount(0), 0u);
	EXPECT_EQ(tracker->destroyedCount(1), 0u);
	pin->release();
	reclaimer.reclaimNow();
	stats = reclaimer.stats();
	EXPECT_EQ(stats.lastScanExamined, 2u);
	EXPECT_EQ(stats.lastScanDestroyed, 2u);
	owner.reset();
	drainReclaimer();
	EXPECT_NO_THROW(tracker->expectAllDestroyedOnce("rebucketed"));
}

// Scans of the Reclaimer thread stop at their budget and the next scan follows without waiting for the period, advancing the epoch each time.
TEST(ReclaimerLivenessTest, ThreadScansAreBoundedAndResumeWithoutWaiting) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	constexpr uint64_t ENTRIES = 50'000;
	constexpr size_t BUDGET = 1000;
	const Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = 500ms;
	config.wakeBacklog = SIZE_MAX;
	config.scanEntryBudget = BUDGET;
	config.scanTimeBudget = std::chrono::microseconds(0);
	{
		TaskScope scope(testTask());
		for (uint64_t i = 0; i < ENTRIES; ++i)
			Reclaimer::retireNode(std::make_unique<ChurnNode>());
	}
	std::atomic<uint64_t> maxExamined{0};
	std::atomic<uint64_t> hookScans{0};
	uint64_t hook = reclaimer.addPostScanHook("liveness", [&] {
		uint64_t examined = Reclaimer::getInstance().stats().lastScanExamined;
		maxExamined = std::max(maxExamined.load(), examined);
		++hookScans;
	});
	const Reclaimer::Stats before = reclaimer.stats();
	reclaimer.start(config);
	const Clock::time_point firstScanDue = Clock::now() + config.period;
	const Clock::time_point deadline = Clock::now() + 20s;
	while (reclaimer.stats().backlog != 0 && Clock::now() < deadline)
		std::this_thread::sleep_for(1ms);
	const Clock::time_point drained = Clock::now();
	reclaimer.stop();
	reclaimer.removePostScanHook(hook);
	reclaimer.configure(original);
	const Reclaimer::Stats after = reclaimer.stats();
	ASSERT_EQ(after.backlog, 0u);
	EXPECT_LE(maxExamined.load(), BUDGET);
	EXPECT_GE(after.scans - before.scans, ENTRIES / BUDGET);
	EXPECT_GE(after.epoch - before.epoch, ENTRIES / BUDGET);
	EXPECT_GE(after.budgetExhaustedScans - before.budgetExhaustedScans, ENTRIES / BUDGET - 1);
	// 50 scans spaced by the 500 ms period would take 25 s
	EXPECT_LT(drained - firstScanDue, 5s) << "budget-limited scans waited for the period";
	std::printf("[liveness] %llu entries in %llu bounded scans, %.0f ms after the first scan was due\n", static_cast<unsigned long long>(ENTRIES),
		static_cast<unsigned long long>(after.scans - before.scans), millisBetween(firstScanDue, drained));
}

namespace {

class ChurnPart final : public OwnedPart {
public:
	explicit ChurnPart(const RefCounted& owner) : OwnedPart(owner) {}

private:
	std::array<uint64_t, 2> payload_{};
};

class ChurnPartOwner final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<ChurnPartOwner> create() { return makeRef<ChurnPartOwner>(); }
	PartMap<int32_t, ChurnPart> map{*this};

protected:
	ChurnPartOwner() = default;
	~ChurnPartOwner() override = default;
};

/** Retires `count` parts of `owner` that stay held by the returned Refs (replaced in the map outside any TaskScope: pushed at once). */
std::vector<Ref<ChurnPart>> retireHeldParts(ChurnPartOwner& owner, int32_t count) {
	for (int32_t i = 0; i < count; ++i)
		owner.map.put(i, std::make_unique<ChurnPart>(owner));
	std::vector<Ref<ChurnPart>> held;
	held.reserve(static_cast<size_t>(count));
	{
		TaskScope scope(testTask());
		for (int32_t i = 0; i < count; ++i)
			held.emplace_back(owner.map.get(i));
	}
	for (int32_t i = 0; i < count; ++i)
		owner.map.put(i, std::make_unique<ChurnPart>(owner));
	return held;
}

} // namespace

// Review finding (2026-09-13): retired OwnedParts that Refs keep alive stay below m and were re-filed into the oldest bucket, so once they filled a
// bounded scan's budget the Reclaimer thread never reached the eligible entries behind them. Bounded scans must keep destroying those entries
// and must still examine the held parts (a part released later is destroyed without reclaimNow).
TEST(ReclaimerLivenessTest, HeldRetiredPartsDoNotStarveBoundedScans) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	constexpr int32_t HELD = 1000;
	constexpr uint64_t NODES = 5000;
	constexpr size_t BUDGET = 64;
	Ref<ChurnPartOwner> owner = ChurnPartOwner::create();
	std::vector<Ref<ChurnPart>> held = retireHeldParts(*owner, HELD);
	detail::testing::advanceEpoch(); // the nodes get a later limbo key than the held parts
	{
		TaskScope scope(testTask());
		for (uint64_t i = 0; i < NODES; ++i)
			Reclaimer::retireNode(std::make_unique<ChurnNode>());
	}
	const Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = 20ms;
	config.wakeBacklog = SIZE_MAX;
	config.scanEntryBudget = BUDGET;
	config.scanTimeBudget = std::chrono::microseconds(0);
	const Reclaimer::Stats before = reclaimer.stats();
	ASSERT_EQ(before.backlog, HELD + NODES);
	reclaimer.start(config);
	const Clock::time_point start = Clock::now();
	auto waitFor = [&](const std::function<bool()>& condition, std::chrono::milliseconds limit) {
		const Clock::time_point deadline = Clock::now() + limit;
		while (!condition() && Clock::now() < deadline)
			std::this_thread::sleep_for(1ms);
		return condition();
	};
	const bool nodesDrained = waitFor([&] { return reclaimer.stats().backlog <= HELD; }, 5000ms);
	const double drainMillis = millisBetween(start, Clock::now());
	const Reclaimer::Stats drained = reclaimer.stats();
	// the parts are released while the thread runs: bounded scans must reach them too
	held.clear();
	const bool partsDrained = waitFor([&] { return reclaimer.stats().backlog == 0; }, 5000ms);
	const Reclaimer::Stats after = reclaimer.stats();
	reclaimer.stop();
	reclaimer.configure(original);
	std::printf("[liveness] %d held parts + %llu nodes, entry budget %zu: nodes %s after %.0f ms (backlog %llu, held parts %llu, %llu scans, %llu "
				"examined); released parts %s (backlog %llu)\n",
		HELD, static_cast<unsigned long long>(NODES), BUDGET, nodesDrained ? "drained" : "NOT drained", drainMillis,
		static_cast<unsigned long long>(drained.backlog), static_cast<unsigned long long>(drained.retiredPartsHeld),
		static_cast<unsigned long long>(drained.scans - before.scans), static_cast<unsigned long long>(drained.examinedTotal - before.examinedTotal),
		partsDrained ? "drained" : "NOT drained", static_cast<unsigned long long>(after.backlog));
	EXPECT_TRUE(nodesDrained) << "held retired parts starved the bounded scans";
	EXPECT_EQ(drained.retiredPartsHeld, static_cast<uint64_t>(HELD)) << "Stats show the retired parts kept by Refs";
	EXPECT_TRUE(partsDrained) << "released parts were not reached by bounded scans";
	EXPECT_EQ(after.retiredPartsHeld, 0u);
	owner.reset();
	drainReclaimer();
}

// Review finding (2026-09-13): budget-limited scans continued without waiting re-took the scan mutex at once, and std::mutex (an SRWLOCK) lets
// the running thread win against a woken waiter, so reclaimNow, removePostScanHook and setDestroyObserver could wait for a whole drain.
TEST(ReclaimerLivenessTest, ContinuedScansLetScanLockWaitersIn) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	const uint64_t nodes = static_cast<uint64_t>(envOr("AION_LIVENESS_WAITER_NODES", 1'000'000));
	const Reclaimer::Config original = reclaimer.getConfig();
	Reclaimer::Config config = original;
	config.period = 20ms;
	config.wakeBacklog = SIZE_MAX;
	config.scanEntryBudget = 1;
	config.scanTimeBudget = std::chrono::microseconds(0);
	config.backlogDumpObjects = SIZE_MAX;
	config.backlogDumpBytes = SIZE_MAX;
	{
		TaskScope scope(testTask());
		for (uint64_t i = 0; i < nodes; ++i)
			Reclaimer::retireNode(std::make_unique<ChurnNode>());
	}
	std::vector<uint64_t> hooks;
	for (int i = 0; i < 40; ++i)
		hooks.push_back(reclaimer.addPostScanHook("waiter", [] {}));
	const uint64_t scansBefore = reclaimer.stats().scans;
	reclaimer.start(config);
	while (reclaimer.stats().scans < scansBefore + 1000 && reclaimer.stats().backlog != 0)
		std::this_thread::sleep_for(1ms);
	std::chrono::milliseconds worst{0};
	uint64_t backlogAfterWaits = 0;
	for (uint64_t hook : hooks) {
		const Clock::time_point begin = Clock::now();
		reclaimer.removePostScanHook(hook); // waits for the running scan (and its hooks)
		worst = std::max(worst, std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - begin));
		backlogAfterWaits = reclaimer.stats().backlog;
		std::this_thread::sleep_for(5ms);
	}
	reclaimer.stop();
	reclaimer.configure(original);
	std::printf("[liveness] scan lock waits during a continued drain of %llu nodes: worst %lld ms, backlog after the waits %llu\n",
		static_cast<unsigned long long>(nodes), static_cast<long long>(worst.count()), static_cast<unsigned long long>(backlogAfterWaits));
	EXPECT_GT(backlogAfterWaits, 0u) << "the drain ended before the waits (scenario too short: raise AION_LIVENESS_WAITER_NODES)";
	EXPECT_LE(worst, 250ms) << "a thread waiting for the scan lock waited for the continued scans";
	drainReclaimer();
}

// The test hook used by the PCT and mutation tests really splits scans: one entry per chunk and per reclaimNow scan.
TEST(ReclaimerLivenessTest, ScanShapeHookSplitsScans) {
	if (AION_LIFETIME_MUTATIONS == 0)
		GTEST_SKIP() << "the scan shape hook is compiled in checked builds only";
	Reclaimer& reclaimer = Reclaimer::getInstance();
	ASSERT_FALSE(reclaimer.isRunning());
	drainReclaimer();
	for (int i = 0; i < 3; ++i)
		Reclaimer::retireNode(std::make_unique<ChurnNode>());
	{
		detail::ScanShapeScope shape(1, 1);
		for (uint64_t left = 2;; --left) {
			reclaimer.reclaimNow();
			EXPECT_EQ(reclaimer.stats().lastScanExamined, 1u);
			EXPECT_EQ(reclaimer.stats().lastScanDestroyed, 1u);
			EXPECT_EQ(reclaimer.stats().backlog, left);
			if (left == 0)
				break;
		}
	}
	for (int i = 0; i < 3; ++i)
		Reclaimer::retireNode(std::make_unique<ChurnNode>());
	reclaimer.reclaimNow();
	EXPECT_EQ(reclaimer.stats().lastScanExamined, 3u) << "the default shape is restored";
}

#if AION_CHECKED
// Review finding (2026-09-13): the C5 double-retire check covered one chunk. A second entry of an object destroyed by an earlier chunk of the
// same scan must terminate before its (freed, possibly reused) memory is read, also without the delayed-free FIFO.
TEST(ReclaimerDeathTest, DuplicateRetireInALaterChunkTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Reclaimer& reclaimer = Reclaimer::getInstance();
			Reclaimer::Config config = reclaimer.getConfig();
			config.delayedFreeBytes = 0;
			reclaimer.configure(config);
			(void)reclaimer.drain(128);
			Ref<ChurnObject> object = ChurnObject::create(1);
			const RefCounted* raw = object.get();
			object.reset(); // last release outside a scope: retired (pushed at once)
			for (int i = 0; i < 100; ++i)
				Reclaimer::retireNode(std::make_unique<ChurnNode>());
			Reclaimer::retire(*raw); // the protocol bug: a second entry, more than one chunk away from the first
			reclaimer.reclaimNow();
		},
		"queued twice");
}
#endif

TEST(ReclaimerLivenessTest, PinnedPublisherDoesNotStallReclamation) {
	runPinnedPublisherScenario(paramsFromEnvironment(Params{.pin = 1500ms, .retiresPerSecond = 60'000, .threads = 4}));
}

TEST(ReclaimerLivenessTest, DISABLED_PinnedPublisherDoesNotStallReclamationLong) {
	runPinnedPublisherScenario(paramsFromEnvironment(Params{.pin = 8000ms, .retiresPerSecond = 150'000, .threads = 8}));
}
