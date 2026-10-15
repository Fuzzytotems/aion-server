// Directed interleavings of the lifetime protocol (design §2.4 safety sketch), each run twice:
// - with the protocol as implemented, the interleaving must be safe (in-process, ASan-clean);
// - with one protocol step removed or replaced (detail::Mutation), the same interleaving must be detected: a reader sees its borrowed object
//   destroyed, a Reclaimer invariant (C5) or cookie check terminates, ASan reports use after free, or the object leaks. Detection runs in a
//   death test child (EXPECT_DEATH), because a broken protocol may corrupt memory.
// A test here fails if the corresponding protocol step is removed from the implementation.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LifetimeTestSupport.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"
#include "support/PctSupport.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;
using detail::Mutation;
using detail::MutationScope;
using pct::ScriptStep;

namespace {

using Bodies = std::vector<std::function<void()>>;

pct::ScheduleResult runScript(Bodies bodies, std::vector<ScriptStep> script) {
	pct::Options options;
	options.script = std::move(script);
	options.keepTrace = true;
	options.timeout = std::chrono::milliseconds(20000);
	return pct::PctScheduler(options).run(std::move(bodies));
}

/** A scenario builds its state, returns the thread bodies and the script, and verifies the end state in `after` (throws on violations). */
struct Scenario {
	Bodies bodies;
	std::vector<ScriptStep> script;
	std::function<void()> after;
};

/** Scan shape of a run (detail::ScanShapeScope): entries per chunk and reclaimNow's entry budget, 0 = default. */
struct ScanShape {
	uint32_t chunk = 0;
	uint64_t reclaimNowEntries = 0;
};

/** Diagnostics: AION_SCAN_SHAPE=chunk1 or budget1 runs every directed scenario (clean and mutated) with split scans. */
ScanShape shapeFromEnvironment() {
	const char* shape = std::getenv("AION_SCAN_SHAPE");
	if (shape != nullptr && std::string(shape) == "chunk1")
		return {1, 0};
	if (shape != nullptr && std::string(shape) == "budget1")
		return {1, 1};
	return {};
}

/** Runs a scenario under `mutation`; returns the schedule result with `after` failures folded in. */
pct::ScheduleResult runScenario(const std::function<Scenario()>& build, Mutation mutation, ScanShape shape = shapeFromEnvironment()) {
	Reclaimer::getInstance().drain(128); // objects of earlier tests
	detail::ScanShapeScope shapeScope(shape.chunk, shape.reclaimNowEntries);
	Scenario scenario = build();
	pct::ScheduleResult result;
	{
		MutationScope scope(mutation);
		result = runScript(std::move(scenario.bodies), std::move(scenario.script));
	}
	if (result.completed && scenario.after) {
		try {
			scenario.after();
		} catch (const std::exception& e) {
			result.completed = false;
			result.failure = std::string("after: ") + e.what();
		}
	}
	return result;
}

/**
 * The clean protocol passes the interleaving; the mutated protocol is detected (death test).
 * Diagnostics: with AION_MUTATION_IN_PROCESS=1 the mutated scenario runs in the test process and its failure is printed (a mutation that
 * corrupts memory may crash the test binary in this mode).
 */
void expectSafeAndMutationDetected(const std::function<Scenario()>& build, Mutation mutation) {
	pct::ScheduleResult clean = runScenario(build, Mutation::NONE);
	EXPECT_TRUE(clean.completed) << testsupport::describeSchedule(clean);
	if (const char* inProcess = std::getenv("AION_MUTATION_IN_PROCESS"); inProcess != nullptr && *inProcess == '1') {
		pct::ScheduleResult mutated = runScenario(build, mutation);
		EXPECT_FALSE(mutated.completed) << "mutation not detected";
		EXPECT_FALSE(testsupport::failedInHarness(mutated)) << testsupport::describeSchedule(mutated);
		std::printf("mutation %u detected: %s\n", static_cast<unsigned>(mutation), mutated.failure.c_str());
		return;
	}
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(testsupport::dieIfFailed(runScenario(build, mutation)), "");
}

/** Creates a queued object with count 1 held by the returned Ref and a retireEpoch older than E by two epochs. */
Ref<Tracked> makeQueuedLiveObject(const std::shared_ptr<Tracker>& tracker, SharedLocation<Tracked>& location) {
	location.store(Tracked::create(tracker));
	Ref<Tracked> keep;
	{
		TaskScope scope(testTask());
		Ptr<Tracked> borrowed = location.load();
		location.store(nullptr); // last release: stamped, queued (retire list of this scope)
		keep = Ref<Tracked>(borrowed); // resurrection 0 -> 1 through the borrow
	} // scope exit flushes: the object is in the incoming stack with queued == true and count == 1
	detail::testing::advanceEpoch();
	detail::testing::advanceEpoch();
	return keep;
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D1: a published reader keeps an unlinked, released object alive across scans.
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario borrowSurvivesUnlinkAndScan() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	state->location.store(Tracked::create(state->tracker));
	enum { READER, WRITER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->location.load();
			pct::yieldPoint("test:borrowed");
			useTracked(*state->tracker, borrowed);
		},
		[state] { state->location.store(nullptr); },
		[] {
			Reclaimer::getInstance().reclaimNow();
			Reclaimer::getInstance().reclaimNow();
		},
	};
	scenario.script = {{READER, "test:borrowed"}, {WRITER, ""}, {SCANNER, ""}, {READER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("borrowSurvivesUnlinkAndScan");
	};
	return scenario;
}

TEST(ProtocolMutationTest, LastReleaseStampProtectsPublishedBorrow) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(borrowSurvivesUnlinkAndScan, Mutation::SKIP_RELEASE_STAMP);
}

TEST(ProtocolMutationTest, MinActiveIncludesPublishedEpochs) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(borrowSurvivesUnlinkAndScan, Mutation::SCAN_IGNORE_PUBLISHED);
}

TEST(ProtocolMutationTest, ReadBarrierPublishesTheEpoch) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(borrowSurvivesUnlinkAndScan, Mutation::PUBLISH_NOTHING);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D2: the stamp is stored before the count CAS. The object is already queued (resurrected earlier) with an old stamp; if the releaser CASes
// 1 -> 0 before stamping, a scan sees count 0 with the old stamp while a newer reader holds a borrow.
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario stampBeforeCas() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	state->location.store(makeQueuedLiveObject(state->tracker, state->location));
	enum { READER, RELEASER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->location.load();
			pct::yieldPoint("test:borrowed");
			useTracked(*state->tracker, borrowed);
		},
		[state] { state->location.store(nullptr); },
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	// clean protocol: paused before the stamp (count still 1); mutated: paused after the 1 -> 0 CAS, before the late stamp
	scenario.script = {{READER, "test:borrowed"}, {RELEASER, "RefCounted::release:stamp"}, {SCANNER, ""}, {READER, ""}, {RELEASER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("stampBeforeCas");
	};
	return scenario;
}

TEST(ProtocolMutationTest, StampIsStoredBeforeTheCountCas) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(stampBeforeCas, Mutation::CAS_BEFORE_STAMP);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D3: count ABA (the counterexample to the design's stampless fast path, see RefCounted.h).
//   Y borrows O from L0. X unlinks L0 and starts the last release (count 1, E = e). Y retains O (count 2) and stores it into L. A scan
//   advances E. Z publishes the newer epoch and borrows O from L. Y unlinks L and releases 2 -> 1. X's CAS 1 -> 0 succeeds (ABA). A scan
//   with m = e_Z must not free O.
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario countAba(const char* releaserPauseSite) {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> first;
		SharedLocation<Tracked> second;
	};
	auto state = std::make_shared<State>();
	state->first.store(Tracked::create(state->tracker));
	enum { Y, X, Z, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->first.load();
			pct::yieldPoint("test:y:loaded");
			state->second.store(Ref<Tracked>(borrowed));
			pct::yieldPoint("test:y:stored");
			state->second.store(nullptr); // unlink + release 2 -> 1
		},
		[state] { state->first.store(nullptr); }, // unlink + last release
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->second.load();
			pct::yieldPoint("test:z:borrowed");
			useTracked(*state->tracker, borrowed);
		},
		[] {
			Reclaimer::getInstance().reclaimNow();
			pct::yieldPoint("test:s:scanned");
			Reclaimer::getInstance().reclaimNow();
		},
	};
	scenario.script = {{Y, "test:y:loaded"}, {X, releaserPauseSite}, {Y, "test:y:stored"}, {SCANNER, "test:s:scanned"}, {Z, "test:z:borrowed"},
		{Y, ""}, {X, ""}, {SCANNER, ""}, {Z, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("countAba");
	};
	return scenario;
}

TEST(ProtocolMutationTest, DesignStamplessFastPathIsUnsafeUnderCountAba) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected([] { return countAba("RefCounted::release:cas"); }, Mutation::DESIGN_STAMPLESS_FAST_PATH);
}

TEST(ProtocolMutationTest, StampIsMonotoneMaximum) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected([] { return countAba("RefCounted::release:stamp"); }, Mutation::STAMP_OVERWRITE);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D4-D7: the queued handshake between the scan and releases of resurrected objects.
// ------------------------------------------------------------------------------------------------------------------------------------------

/**
 * D4: a resurrected object is dropped from the queue only after clearing `queued`, so its next last release pushes it again. The resurrector
 * ends its task while it holds the Ref: a scan examines only entries whose limbo key is below its m (Reclaimer.h), so the scan must not be held
 * back by the resurrector's own publication to reach the resurrected entry (with the task still published, the scan skips the entry and a
 * later scan finds it released again, which is safe whether or not `queued` was cleared).
 */
Scenario resurrectedObjectIsRequeued() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	state->location.store(Tracked::create(state->tracker));
	enum { RESURRECTOR, WRITER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			Ref<Tracked> keep;
			{
				TaskScope scope(testTask());
				Ptr<Tracked> borrowed = state->location.load();
				pct::yieldPoint("test:r:loaded");
				keep = Ref<Tracked>(borrowed); // 0 -> 1
			}
			pct::yieldPoint("test:r:resurrected");
			keep.reset(); // 1 -> 0 again
		},
		[state] { state->location.store(nullptr); },
		[state] {
			Reclaimer::getInstance().reclaimNow();
			pct::yieldPoint("test:s:scanned");
			Reclaimer::getInstance().drain(128);
			state->tracker->expectAllDestroyedOnce("resurrectedObjectIsRequeued");
		},
	};
	scenario.script = {{RESURRECTOR, "test:r:loaded"}, {WRITER, ""}, {RESURRECTOR, "test:r:resurrected"}, {SCANNER, "test:s:scanned"},
		{RESURRECTOR, ""}, {SCANNER, ""}};
	return scenario;
}

TEST(ProtocolMutationTest, ScanClearsQueuedOfResurrectedObjects) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(resurrectedObjectIsRequeued, Mutation::SCAN_KEEP_WITHOUT_CLEAR);
}

/** D5/D6/D7: the object is queued and alive (count 1 held by `keep`); the scan and the last release race on `queued`. */
Scenario queuedHandshake(const char* scannerPauseSite) {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
		Ref<Tracked> keep;
	};
	auto state = std::make_shared<State>();
	state->keep = makeQueuedLiveObject(state->tracker, state->location);
	enum { RELEASER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] { state->keep.reset(); },
		[state] {
			Reclaimer::getInstance().reclaimNow();
			Reclaimer::getInstance().drain(128);
			state->tracker->expectAllDestroyedOnce("queuedHandshake");
		},
	};
	if (scannerPauseSite != nullptr)
		scenario.script = {{SCANNER, scannerPauseSite}, {RELEASER, ""}, {SCANNER, ""}};
	else
		scenario.script = {{RELEASER, ""}, {SCANNER, ""}};
	return scenario;
}

TEST(ProtocolMutationTest, ScanRechecksCountAfterClearingQueued) {
	AION_SKIP_WITHOUT_PCT();
	// the release runs between the scan's count read (> 0) and its clear: its queued.exchange sees true and does not push
	expectSafeAndMutationDetected([] { return queuedHandshake("Reclaimer::scan:clearQueued"); }, Mutation::SCAN_DROP_WITHOUT_RECHECK);
}

TEST(ProtocolMutationTest, ScanRequeuesOnlyThroughTheQueuedExchange) {
	AION_SKIP_WITHOUT_PCT();
	// the release runs after the clear: it pushes the object itself, so the scan must not keep a second entry
	expectSafeAndMutationDetected([] { return queuedHandshake("Reclaimer::scan:recheck"); }, Mutation::SCAN_REQUEUE_WITHOUT_EXCHANGE);
}

TEST(ProtocolMutationTest, LastReleasePushesOnlyThroughTheQueuedExchange) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected([] { return queuedHandshake(nullptr); }, Mutation::RELEASE_ALWAYS_PUSH);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D8: decrements are CASes (a lost decrement leaks the object).
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario concurrentDecrements() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
		Ref<Tracked> first;
		Ref<Tracked> second;
	};
	auto state = std::make_shared<State>();
	state->location.store(Tracked::create(state->tracker));
	{
		TaskScope scope(testTask());
		state->first = Ref<Tracked>(state->location.load());
		state->second = state->first;
	}
	enum { FIRST, SECOND, FINISHER };
	Scenario scenario;
	scenario.bodies = {
		[state] { state->first.reset(); },
		[state] { state->second.reset(); },
		[state] {
			state->location.store(nullptr);
			Reclaimer::getInstance().drain(128);
			state->tracker->expectAllDestroyedOnce("concurrentDecrements");
		},
	};
	scenario.script = {{FIRST, "RefCounted::release:cas"}, {SECOND, ""}, {FIRST, ""}, {FINISHER, ""}};
	return scenario;
}

TEST(ProtocolMutationTest, DecrementsAreCompareAndSwap) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(concurrentDecrements, Mutation::NON_ATOMIC_DECREMENT);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D9: m is computed before any object is read. The scan reads a zero count, a published resurrector stores the object again and ends its
// task, a newer reader borrows it; an m computed now would free a live object.
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario minBeforeObjects() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> first;
		SharedLocation<Tracked> second;
	};
	auto state = std::make_shared<State>();
	state->first.store(Tracked::create(state->tracker));
	enum { RESURRECTOR, WRITER, SCANNER, READER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->first.load();
			pct::yieldPoint("test:r:loaded");
			state->second.store(Ref<Tracked>(borrowed));
		},
		[state] { state->first.store(nullptr); },
		[] { Reclaimer::getInstance().reclaimNow(); },
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->second.load();
			pct::yieldPoint("test:z:borrowed");
			useTracked(*state->tracker, borrowed);
		},
	};
	scenario.script = {{RESURRECTOR, "test:r:loaded"}, {WRITER, ""}, {SCANNER, "Reclaimer::scan:readPublished"}, {RESURRECTOR, ""},
		{READER, "test:z:borrowed"}, {SCANNER, ""}, {READER, ""}};
	scenario.after = [state] {
		state->second.store(nullptr);
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("minBeforeObjects");
	};
	return scenario;
}

TEST(ProtocolMutationTest, MinActiveIsComputedBeforeReadingObjects) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(minBeforeObjects, Mutation::SCAN_MIN_AFTER_OBJECTS);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D10/D11: retirePart and retireNode stamp the epoch at retire time (C12).
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario retiredPartSurvivesBorrow() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		Ref<PartOwner> owner = PartOwner::create();
	};
	auto state = std::make_shared<State>();
	state->owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*state->owner, state->tracker));
	enum { READER, WRITER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			TrackedPart* part = state->owner->reclaimerSlot.get();
			pct::yieldPoint("test:borrowed");
			useTrackedPart(*state->tracker, part);
		},
		[state] { state->owner->reclaimerSlot.set(nullptr); },
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	scenario.script = {{READER, "test:borrowed"}, {WRITER, ""}, {SCANNER, ""}, {READER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("retiredPartSurvivesBorrow");
		if (state->owner->refCount() != 1)
			throw LifetimeViolation("the retired part still retains its owner");
	};
	return scenario;
}

TEST(ProtocolMutationTest, RetirePartStampsTheCurrentEpoch) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(retiredPartSurvivesBorrow, Mutation::PART_STAMP_ZERO);
}

Scenario retiredNodeSurvivesLoad() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		NodeLocation location;
	};
	auto state = std::make_shared<State>();
	state->location.replace(std::make_unique<TrackedNode>(state->tracker));
	enum { READER, WRITER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			const TrackedNode* node = state->location.load();
			pct::yieldPoint("test:borrowed");
			useTrackedNode(*state->tracker, node);
		},
		[state] { state->location.replace(nullptr); },
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	scenario.script = {{READER, "test:borrowed"}, {WRITER, ""}, {SCANNER, ""}, {READER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("retiredNodeSurvivesLoad");
	};
	return scenario;
}

TEST(ProtocolMutationTest, RetireNodeStampsTheCurrentEpoch) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(retiredNodeSurvivesLoad, Mutation::NODE_STAMP_ZERO);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D12: quiescentPoint() in a nested scope is a no-op (the enclosing frame's borrow stays protected, RR-15).
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario nestedQuiescentPointKeepsBorrows() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	state->location.store(Tracked::create(state->tracker));
	enum { READER, WRITER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			QuiescentScope quiescent;
			Ptr<Tracked> borrowed = state->location.load();
			{
				TaskScope nested(testTask());
				quiescentPoint(); // depth 2: must not unpublish
			}
			pct::yieldPoint("test:after-quiescent");
			useTracked(*state->tracker, borrowed);
		},
		[state] { state->location.store(nullptr); },
		[] { Reclaimer::getInstance().reclaimNow(); },
	};
	scenario.script = {{READER, "test:after-quiescent"}, {WRITER, ""}, {SCANNER, ""}, {READER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("nestedQuiescentPointKeepsBorrows");
	};
	return scenario;
}

TEST(ProtocolMutationTest, QuiescentPointRulesProtectEnclosingBorrows) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(nestedQuiescentPointKeepsBorrows, Mutation::QUIESCENT_IGNORE_RULES);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D13/D14: liveness steps (without them objects are never reclaimed).
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario releasedObjectIsReclaimed() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	state->location.store(Tracked::create(state->tracker));
	Scenario scenario;
	scenario.bodies = {
		[state] {
			{
				TaskScope scope(testTask());
				useTracked(*state->tracker, state->location.load());
			}
			state->location.store(nullptr);
			Reclaimer::getInstance().drain(16);
			state->tracker->expectAllDestroyedOnce("releasedObjectIsReclaimed");
		},
	};
	return scenario;
}

TEST(ProtocolMutationTest, OutermostScopeExitUnpublishes) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(releasedObjectIsReclaimed, Mutation::SCOPE_EXIT_NO_UNPUBLISH);
}

TEST(ProtocolMutationTest, ScansAdvanceTheEpoch) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(releasedObjectIsReclaimed, Mutation::SCAN_NO_ADVANCE);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D15: the re-check loop of lazy publication is not a safety step of this protocol (documented at Mutation::PUBLISH_NO_RECHECK). The reader
// loads E, a scan advances E without seeing the reader, the reader publishes the stale epoch and borrows; the borrow stays safe.
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario stalePublication() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	state->location.store(Tracked::create(state->tracker));
	enum { READER, WRITER, SCANNER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> borrowed = state->location.load();
			pct::yieldPoint("test:borrowed");
			useTracked(*state->tracker, borrowed);
		},
		[state] { state->location.store(nullptr); },
		[] {
			Reclaimer::getInstance().reclaimNow();
			pct::yieldPoint("test:s:scanned");
			Reclaimer::getInstance().reclaimNow();
		},
	};
	scenario.script = {{READER, "TaskScope::ensurePublished:store"}, {SCANNER, "test:s:scanned"}, {READER, "test:borrowed"}, {WRITER, ""},
		{SCANNER, ""}, {READER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("stalePublication");
	};
	return scenario;
}

TEST(ProtocolMutationTest, PublicationRecheckIsNotASafetyStep) {
	AION_SKIP_WITHOUT_PCT();
	pct::ScheduleResult clean = runScenario(stalePublication, Mutation::NONE);
	EXPECT_TRUE(clean.completed) << testsupport::describeSchedule(clean);
	pct::ScheduleResult mutated = runScenario(stalePublication, Mutation::PUBLISH_NO_RECHECK);
	EXPECT_TRUE(mutated.completed) << testsupport::describeSchedule(mutated);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D16 (review): a Ptr made from a Ref the thread owns publishes. The task has loaded nothing; it borrows its own new object, moves the Ref into
// a shared location and keeps using the Ptr while another thread unlinks and releases the object and scans (design §2.4 let only pointer
// loads publish, so the scan saw no published epoch and freed the object).
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario ptrFromOwnedRefSurvivesMoveUnlinkAndScan() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
	};
	auto state = std::make_shared<State>();
	enum { OWNER, WRITER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ref<Tracked> local = Tracked::create(state->tracker);
			Ptr<Tracked> borrowed = local; // must publish: nothing else in this task does
			state->location.store(std::move(local));
			pct::yieldPoint("test:stored");
			useTracked(*state->tracker, borrowed);
		},
		[state] {
			state->location.store(nullptr); // last release outside any scope: pushed at once
			Reclaimer::getInstance().reclaimNow();
			Reclaimer::getInstance().reclaimNow();
		},
	};
	scenario.script = {{OWNER, "test:stored"}, {WRITER, ""}, {OWNER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("ptrFromOwnedRefSurvivesMoveUnlinkAndScan");
	};
	return scenario;
}

TEST(ProtocolMutationTest, PtrFromAnOwnedRefPublishes) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(ptrFromOwnedRefSurvivesMoveUnlinkAndScan, Mutation::BORROW_NO_PUBLISH);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D17 (review): a Ptr bound to the temporary Ref returned by getAndSet/exchange (`Ptr<Future> t = task.getAndSet(nullptr)`) publishes before
// the temporary is released; the last reference is then released by another thread, which scans.
// ------------------------------------------------------------------------------------------------------------------------------------------
Scenario ptrFromTemporaryRefSurvivesCrossThreadLastRelease() {
	struct State {
		std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
		SharedLocation<Tracked> location;
		Ref<Tracked> keeper;
	};
	auto state = std::make_shared<State>();
	state->keeper = Tracked::create(state->tracker);
	state->location.store(state->keeper);
	enum { TAKER, RELEASER };
	Scenario scenario;
	scenario.bodies = {
		[state] {
			TaskScope scope(testTask());
			Ptr<Tracked> taken = state->location.exchange(nullptr); // the temporary Ref dies at the end of this statement (2 -> 1)
			pct::yieldPoint("test:taken");
			useTracked(*state->tracker, taken);
		},
		[state] {
			state->keeper.reset(); // 1 -> 0 outside any scope
			Reclaimer::getInstance().reclaimNow();
			Reclaimer::getInstance().reclaimNow();
		},
	};
	scenario.script = {{TAKER, "test:taken"}, {RELEASER, ""}, {TAKER, ""}};
	scenario.after = [state] {
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("ptrFromTemporaryRefSurvivesCrossThreadLastRelease");
	};
	return scenario;
}

TEST(ProtocolMutationTest, PtrFromATemporaryRefPublishes) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected(ptrFromTemporaryRefSurvivesCrossThreadLastRelease, Mutation::BORROW_NO_PUBLISH);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// D18 (review): a Ref to a part keeps the part alive after the part was replaced in a PartMap (Player.playerAccountData of a stale Player after
// Account.addPlayerAccountData), and a borrow taken from that Ref after the retirement stays valid after the Ref's release (the release stamps
// the part).
// ------------------------------------------------------------------------------------------------------------------------------------------
struct RetiredPartState {
	std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
};

Scenario refToReplacedPartKeepsItAlive(bool borrowAfterRetirement) {
	auto state = std::make_shared<RetiredPartState>();
	state->owner->map.put(1, std::make_unique<TrackedPart>(*state->owner, state->tracker)); // part 0
	Scenario scenario;
	scenario.bodies = {
		[state, borrowAfterRetirement] {
			Ref<TrackedPart> held;
			{
				TaskScope scope(testTask());
				held = Ref<TrackedPart>(state->owner->map.get(1));
			}
			state->owner->map.put(1, std::make_unique<TrackedPart>(*state->owner, state->tracker)); // retires part 0 (outside scopes: pushed)
			Reclaimer::getInstance().reclaimNow();
			Reclaimer::getInstance().reclaimNow();
			TaskScope scope(testTask());
			useTrackedPart(*state->tracker, held.get());
			if (borrowAfterRetirement) {
				Ptr<TrackedPart> borrowed = held; // publishes an epoch newer than the retirement stamp
				held.reset();                     // must stamp the part
				Reclaimer::getInstance().reclaimNow();
				Reclaimer::getInstance().reclaimNow();
				useTrackedPart(*state->tracker, borrowed.rawPointer());
			}
		},
	};
	scenario.script = {{0, ""}};
	scenario.after = [state] {
		state->owner.reset();
		Reclaimer::getInstance().drain(128);
		state->tracker->expectAllDestroyedOnce("refToReplacedPartKeepsItAlive");
	};
	return scenario;
}

TEST(ProtocolMutationTest, RefToAReplacedPartKeepsThePartAlive) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected([] { return refToReplacedPartKeepsItAlive(false); }, Mutation::PART_REFS_IGNORED);
}

TEST(ProtocolMutationTest, ReleaseOfARefToARetiredPartStampsThePart) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected([] { return refToReplacedPartKeepsItAlive(true); }, Mutation::PART_RELEASE_NO_STAMP);
}

TEST(ProtocolMutationTest, BorrowFromARefToARetiredPartPublishes) {
	AION_SKIP_WITHOUT_PCT();
	expectSafeAndMutationDetected([] { return refToReplacedPartKeepsItAlive(true); }, Mutation::BORROW_NO_PUBLISH);
}

// Review finding (2026-09-13): every directed interleaving stays safe when scans destroy after each classified entry and reclaimNow examines one
// entry per scan (destructors, cascades and budget cuts interleave with the classification of later entries of the same or the next scan).
TEST(ProtocolMutationTest, DirectedScenariosStaySafeUnderSplitScans) {
	AION_SKIP_WITHOUT_PCT();
	const std::vector<std::pair<const char*, std::function<Scenario()>>> scenarios = {
		{"borrowSurvivesUnlinkAndScan", borrowSurvivesUnlinkAndScan},
		{"stampBeforeCas", stampBeforeCas},
		{"countAba(cas)", [] { return countAba("RefCounted::release:cas"); }},
		{"countAba(stamp)", [] { return countAba("RefCounted::release:stamp"); }},
		{"resurrectedObjectIsRequeued", resurrectedObjectIsRequeued},
		{"queuedHandshake(clearQueued)", [] { return queuedHandshake("Reclaimer::scan:clearQueued"); }},
		{"queuedHandshake(recheck)", [] { return queuedHandshake("Reclaimer::scan:recheck"); }},
		{"queuedHandshake(none)", [] { return queuedHandshake(nullptr); }},
		{"concurrentDecrements", concurrentDecrements},
		{"minBeforeObjects", minBeforeObjects},
		{"retiredPartSurvivesBorrow", retiredPartSurvivesBorrow},
		{"retiredNodeSurvivesLoad", retiredNodeSurvivesLoad},
		{"nestedQuiescentPointKeepsBorrows", nestedQuiescentPointKeepsBorrows},
		{"releasedObjectIsReclaimed", releasedObjectIsReclaimed},
		{"stalePublication", stalePublication},
		{"ptrFromOwnedRefSurvivesMoveUnlinkAndScan", ptrFromOwnedRefSurvivesMoveUnlinkAndScan},
		{"ptrFromTemporaryRefSurvivesCrossThreadLastRelease", ptrFromTemporaryRefSurvivesCrossThreadLastRelease},
		{"refToReplacedPartKeepsItAlive(false)", [] { return refToReplacedPartKeepsItAlive(false); }},
		{"refToReplacedPartKeepsItAlive(true)", [] { return refToReplacedPartKeepsItAlive(true); }},
	};
	for (const ScanShape shape : {ScanShape{1, 0}, ScanShape{1, 1}}) {
		for (const auto& [name, build] : scenarios) {
			pct::ScheduleResult result = runScenario(build, Mutation::NONE, shape);
			EXPECT_TRUE(result.completed) << name << " (chunk " << shape.chunk << ", reclaimNow budget " << shape.reclaimNowEntries
										  << "): " << testsupport::describeSchedule(result);
		}
	}
}

} // namespace
