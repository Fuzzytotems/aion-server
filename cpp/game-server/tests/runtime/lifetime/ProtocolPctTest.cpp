// Randomized PCT exploration of the lifetime protocol (design §12.4 scenarios): release (fast path and last) vs resurrection vs scan, lazy
// publication vs location exchange, count ABA, retirePart vs borrow, SelfOrRef stores vs borrow, QuiescentScope loops vs writers, PartMap
// replacement vs readers. Every schedule checks that no borrowed object was destroyed and that every object is destroyed exactly once after
// quiesce. Defaults keep the suite fast; nightly runs set AION_PCT_SCHEDULES (e.g. 100000).
// The last tests prove that random exploration (not only the directed scripts) finds a removed protocol step.

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "LifetimeTestSupport.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"
#include "support/PctSupport.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

namespace {

using Bodies = std::vector<std::function<void()>>;

constexpr uint32_t DEFAULT_SCHEDULES = 150;

pct::ScheduleResult exploreScenario(uint32_t schedules, const pct::ScenarioFactory& factory, const std::function<void()>& check) {
	Reclaimer::getInstance().drain(128);
	pct::Options options;
	options.depth = 3;
	options.timeout = std::chrono::milliseconds(20000);
	return pct::explore(schedules, 1, factory, check, options);
}

/**
 * Explores the scenario with the default scan shape and with split scans (review finding 2026-09-13): one entry per chunk (destruction after
 * every classified entry), and additionally one entry per reclaimNow scan (budget cuts between entries, the next scan computes a new m).
 */
pct::ScheduleResult exploreScenarioInAllScanShapes(uint32_t schedules, const pct::ScenarioFactory& factory, const std::function<void()>& check) {
	struct Shape {
		const char* name;
		uint32_t chunk;
		uint64_t reclaimNowEntries;
	};
	pct::ScheduleResult result;
	for (const Shape& shape : {Shape{"default", 0, 0}, Shape{"chunk 1", 1, 0}, Shape{"chunk 1, reclaimNow budget 1 entry", 1, 1}}) {
		detail::ScanShapeScope scope(shape.chunk, shape.reclaimNowEntries);
		result = exploreScenario(schedules, factory, check);
		if (!result.completed) {
			result.failure = std::string("[scan shape ") + shape.name + "] " + result.failure;
			return result;
		}
	}
	return result;
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// release vs resurrection vs scan
// ------------------------------------------------------------------------------------------------------------------------------------------
struct ResurrectionState {
	std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
	SharedLocation<Tracked> first;
	SharedLocation<Tracked> second;
};

pct::ScenarioFactory resurrectionScenario(std::shared_ptr<ResurrectionState>& current) {
	return [&current](uint64_t) {
		auto state = current = std::make_shared<ResurrectionState>();
		state->first.store(Tracked::create(state->tracker));
		return Bodies{
			[state] { // borrow, use, resurrect into another location
				TaskScope scope(testTask());
				Ptr<Tracked> borrowed = state->first.load();
				useTracked(*state->tracker, borrowed);
				if (borrowed) {
					state->second.store(Ref<Tracked>(borrowed));
					useTracked(*state->tracker, borrowed);
				}
			},
			[state] { state->first.store(nullptr); },
			[state] {
				{
					TaskScope scope(testTask());
					Ptr<Tracked> borrowed = state->second.load();
					pct::yieldPoint("test:z:borrowed");
					useTracked(*state->tracker, borrowed);
				}
				state->second.store(nullptr);
			},
			[] {
				Reclaimer::getInstance().reclaimNow();
				Reclaimer::getInstance().reclaimNow();
			},
		};
	};
}

void checkResurrection(const std::shared_ptr<ResurrectionState>& state) {
	state->first.store(nullptr);
	state->second.store(nullptr);
	Reclaimer::getInstance().drain(128);
	state->tracker->expectAllDestroyedOnce("resurrection");
}

TEST(ProtocolPctTest, ReleaseResurrectionAndScan) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<ResurrectionState> state;
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), resurrectionScenario(state), [&] { checkResurrection(state); });
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// count ABA: a copy is stored into a second location and unlinked again while the last holder releases
// ------------------------------------------------------------------------------------------------------------------------------------------
TEST(ProtocolPctTest, CountAbaAcrossTwoLocations) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<ResurrectionState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<ResurrectionState>();
		state->first.store(Tracked::create(state->tracker));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				Ptr<Tracked> borrowed = state->first.load();
				if (borrowed) {
					state->second.store(Ref<Tracked>(borrowed));
					pct::yieldPoint("test:y:stored");
					state->second.store(nullptr);
				}
			},
			[state] { state->first.store(nullptr); },
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
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, [&] { checkResurrection(current); });
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// lazy publication vs location exchange with fresh objects
// ------------------------------------------------------------------------------------------------------------------------------------------
TEST(ProtocolPctTest, LazyPublicationVersusExchange) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<ResurrectionState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<ResurrectionState>();
		state->first.store(Tracked::create(state->tracker));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				for (int i = 0; i < 3; ++i)
					useTracked(*state->tracker, state->first.load());
			},
			[state] {
				TaskScope scope(testTask());
				for (int i = 0; i < 2; ++i)
					useTracked(*state->tracker, state->first.load());
			},
			[state] {
				for (int i = 0; i < 3; ++i)
					state->first.store(Tracked::create(state->tracker));
			},
			[] {
				for (int i = 0; i < 3; ++i)
					Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, [&] { checkResurrection(current); });
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// cascades: destroying a parent releases its child at the current epoch while a reader borrows the child
// ------------------------------------------------------------------------------------------------------------------------------------------
TEST(ProtocolPctTest, CascadingReleaseVersusBorrow) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<ResurrectionState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<ResurrectionState>();
		Ref<Tracked> child = Tracked::create(state->tracker);
		state->second.store(child);
		state->first.store(Tracked::create(state->tracker, std::move(child)));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				Ptr<Tracked> borrowed = state->second.load();
				pct::yieldPoint("test:borrowed");
				useTracked(*state->tracker, borrowed);
			},
			[state] {
				state->first.store(nullptr);
				state->second.store(nullptr);
			},
			[] {
				for (int i = 0; i < 3; ++i)
					Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, [&] { checkResurrection(current); });
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// retirePart (PartSlot RECLAIMER) and retireNode vs borrow
// ------------------------------------------------------------------------------------------------------------------------------------------
struct PartState {
	std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	NodeLocation nodes;
};

TEST(ProtocolPctTest, RetiredPartsAndNodesVersusBorrow) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<PartState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<PartState>();
		state->owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*state->owner, state->tracker));
		state->nodes.replace(std::make_unique<TrackedNode>(state->tracker));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				TrackedPart* part = state->owner->reclaimerSlot.get();
				const TrackedNode* node = state->nodes.load();
				pct::yieldPoint("test:borrowed");
				useTrackedPart(*state->tracker, part);
				useTrackedNode(*state->tracker, node);
			},
			[state] {
				state->owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*state->owner, state->tracker));
				state->nodes.replace(std::make_unique<TrackedNode>(state->tracker));
				state->owner->reclaimerSlot.set(nullptr);
			},
			[] {
				Reclaimer::getInstance().reclaimNow();
				Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	auto check = [&current] {
		current->nodes.replace(nullptr);
		Reclaimer::getInstance().drain(128);
		current->tracker->expectAllDestroyedOnce("parts");
		if (current->owner->refCount() != 1)
			throw LifetimeViolation("retired parts still retain the owner");
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, check);
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// SelfOrRef: self (non-retaining) and foreign (retaining) stores, exchange and compareAndSet vs borrow
// ------------------------------------------------------------------------------------------------------------------------------------------
struct SelfOrRefState {
	std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
	Ref<Tracked> self;
	SharedLocation<Tracked> foreign;
};

TEST(ProtocolPctTest, SelfOrRefStoresVersusBorrow) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<SelfOrRefState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<SelfOrRefState>();
		state->self = Tracked::create(state->tracker);
		state->foreign.store(Tracked::create(state->tracker));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				for (int i = 0; i < 2; ++i) {
					Ptr<Tracked> target = state->self->target.get();
					pct::yieldPoint("test:borrowed");
					useTracked(*state->tracker, target);
				}
			},
			[state] {
				TaskScope scope(testTask());
				Ptr<Tracked> foreign = state->foreign.load();
				state->self->target = foreign;
				Ref<Tracked> previous = state->self->target.exchange(Ptr<Tracked>(*state->self));
				(void)state->self->target.compareAndSet(Ptr<Tracked>(*state->self), foreign);
			},
			[state] { state->foreign.store(nullptr); },
			[state] {
				TaskScope scope(testTask());
				(void)state->self->target.compareAndSet(state->self->target.get(), nullptr);
			},
			[] {
				Reclaimer::getInstance().reclaimNow();
				Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	auto check = [&current] {
		current->self->target = nullptr;
		current->self = nullptr;
		current->foreign.store(nullptr);
		Reclaimer::getInstance().drain(128);
		current->tracker->expectAllDestroyedOnce("SelfOrRef");
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, check);
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// QuiescentScope loop (quiescentPoint per element over a Ref snapshot) vs writers and scans
// ------------------------------------------------------------------------------------------------------------------------------------------
TEST(ProtocolPctTest, QuiescentLoopVersusWriters) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<ResurrectionState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<ResurrectionState>();
		state->first.store(Tracked::create(state->tracker));
		state->second.store(Tracked::create(state->tracker));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				QuiescentScope quiescent;
				std::vector<Ref<Tracked>> snapshot;
				snapshot.emplace_back(state->first.load());
				snapshot.emplace_back(state->second.load());
				for (const Ref<Tracked>& element : snapshot) {
					quiescentPoint();
					useTracked(*state->tracker, element);          // kept alive by the snapshot Ref
					useTracked(*state->tracker, state->first.load()); // fresh borrow after the quiescent point
				}
			},
			[state] {
				state->first.store(nullptr);
				state->second.store(Tracked::create(state->tracker));
			},
			[] {
				Reclaimer::getInstance().reclaimNow();
				Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, [&] { checkResurrection(current); });
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// PartMap replacement and removal vs readers (Monitor-serialized writers, epoch-protected borrows)
// ------------------------------------------------------------------------------------------------------------------------------------------
TEST(ProtocolPctTest, PartMapReplacementVersusReaders) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<PartState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<PartState>();
		state->owner->map.put(1, std::make_unique<TrackedPart>(*state->owner, state->tracker));
		return Bodies{
			[state] {
				TaskScope scope(testTask());
				Ptr<TrackedPart> part = state->owner->map.get(1);
				pct::yieldPoint("test:borrowed");
				useTrackedPart(*state->tracker, part.rawPointer());
				for (Ptr<TrackedPart> value : state->owner->map.values())
					useTrackedPart(*state->tracker, value.rawPointer());
			},
			[state] {
				state->owner->map.put(1, std::make_unique<TrackedPart>(*state->owner, state->tracker));
				(void)state->owner->map.remove(1);
			},
			[] {
				Reclaimer::getInstance().reclaimNow();
				Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	auto check = [&current] {
		Reclaimer::getInstance().drain(128);
		current->tracker->expectAllDestroyedOnce("PartMap");
		if (current->owner->refCount() != 1)
			throw LifetimeViolation("retired map parts still retain the owner");
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, check);
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// many entries per scan (review finding 2026-09-13): parents with children (cascades released by destructors of earlier chunks), a resurrected
// child, a replaced part held by a Ref and retired nodes, so split scans destroy and classify interleaved within and across scans
// ------------------------------------------------------------------------------------------------------------------------------------------
struct ManyEntriesState {
	std::shared_ptr<Tracker> tracker = std::make_shared<Tracker>();
	std::array<SharedLocation<Tracked>, 3> parents;
	std::array<SharedLocation<Tracked>, 3> children;
	SharedLocation<Tracked> resurrected;
	Ref<PartOwner> owner = PartOwner::create();
	NodeLocation nodes;
};

TEST(ProtocolPctTest, ManyEntriesVersusBorrowsUnderSplitScans) {
	AION_SKIP_WITHOUT_PCT();
	std::shared_ptr<ManyEntriesState> current;
	pct::ScenarioFactory factory = [&current](uint64_t) {
		auto state = current = std::make_shared<ManyEntriesState>();
		for (size_t i = 0; i < state->parents.size(); ++i) {
			Ref<Tracked> child = Tracked::create(state->tracker);
			state->children[i].store(child);
			state->parents[i].store(Tracked::create(state->tracker, std::move(child)));
		}
		state->owner->map.put(1, std::make_unique<TrackedPart>(*state->owner, state->tracker));
		state->nodes.replace(std::make_unique<TrackedNode>(state->tracker));
		return Bodies{
			[state] { // reader: children, the part and the node
				TaskScope scope(testTask());
				Ptr<Tracked> first = state->children[0].load();
				Ptr<TrackedPart> part = state->owner->map.get(1);
				pct::yieldPoint("test:borrowed");
				useTracked(*state->tracker, first);
				useTrackedPart(*state->tracker, part.rawPointer());
				useTracked(*state->tracker, state->children[1].load());
				useTrackedNode(*state->tracker, state->nodes.load());
			},
			[state] { // resurrector: a child into another location, and a Ref to the part that outlives its replacement
				Ref<TrackedPart> held;
				{
					TaskScope scope(testTask());
					Ptr<Tracked> child = state->children[2].load();
					if (child)
						state->resurrected.store(Ref<Tracked>(child));
					held = Ref<TrackedPart>(state->owner->map.get(1));
				}
				pct::yieldPoint("test:holding");
				state->resurrected.store(nullptr);
				{
					TaskScope scope(testTask());
					useTrackedPart(*state->tracker, Ptr<TrackedPart>(held).rawPointer());
				}
			},
			[state] { // writer: unlinks everything, replaces the part and the node
				for (size_t i = 0; i < state->parents.size(); ++i) {
					state->parents[i].store(nullptr);
					state->children[i].store(nullptr);
				}
				state->owner->map.put(1, std::make_unique<TrackedPart>(*state->owner, state->tracker));
				state->nodes.replace(std::make_unique<TrackedNode>(state->tracker));
			},
			[] {
				for (int i = 0; i < 4; ++i)
					Reclaimer::getInstance().reclaimNow();
			},
		};
	};
	auto check = [&current] {
		current->resurrected.store(nullptr);
		current->nodes.replace(nullptr);
		(void)current->owner->map.remove(1);
		Reclaimer::getInstance().drain(128);
		current->tracker->expectAllDestroyedOnce("many entries");
		if (current->owner->refCount() != 1)
			throw LifetimeViolation("retired parts still retain the owner");
	};
	pct::ScheduleResult result = exploreScenarioInAllScanShapes(testsupport::pctSchedules(DEFAULT_SCHEDULES), factory, check);
	AION_EXPECT_SCHEDULE_OK(result);
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// random exploration also finds removed steps
// ------------------------------------------------------------------------------------------------------------------------------------------
pct::ScheduleResult exploreMutatedResurrection(detail::Mutation mutation, bool splitScans) {
	std::shared_ptr<ResurrectionState> state;
	detail::ScanShapeScope shape(splitScans ? 1 : 0, splitScans ? 1 : 0);
	detail::MutationScope scope(mutation);
	return exploreScenario(2000, resurrectionScenario(state), [&] { checkResurrection(state); });
}

/** Death test; with AION_MUTATION_IN_PROCESS=1 the exploration runs in the test process and prints the detecting schedule (diagnostics). */
void expectRandomExplorationFinds(detail::Mutation mutation, bool splitScans = false) {
	if (const char* inProcess = std::getenv("AION_MUTATION_IN_PROCESS"); inProcess != nullptr && *inProcess == '1') {
		pct::ScheduleResult result = exploreMutatedResurrection(mutation, splitScans);
		EXPECT_FALSE(result.completed);
		std::printf("mutation %u found by random exploration: %s\n", static_cast<unsigned>(mutation), testsupport::describeSchedule(result).c_str());
		return;
	}
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(testsupport::dieIfFailed(exploreMutatedResurrection(mutation, splitScans)), "");
}

TEST(ProtocolPctTest, RandomExplorationFindsMissingReleaseStamp) {
	AION_SKIP_WITHOUT_PCT();
	expectRandomExplorationFinds(detail::Mutation::SKIP_RELEASE_STAMP);
}

TEST(ProtocolPctTest, RandomExplorationFindsIgnoredPublications) {
	AION_SKIP_WITHOUT_PCT();
	expectRandomExplorationFinds(detail::Mutation::SCAN_IGNORE_PUBLISHED);
}

// the split scan shapes do not hide a removed step from random exploration
TEST(ProtocolPctTest, RandomExplorationFindsMissingReleaseStampUnderSplitScans) {
	AION_SKIP_WITHOUT_PCT();
	expectRandomExplorationFinds(detail::Mutation::SKIP_RELEASE_STAMP, true);
}

TEST(ProtocolPctTest, RandomExplorationFindsIgnoredPublicationsUnderSplitScans) {
	AION_SKIP_WITHOUT_PCT();
	expectRandomExplorationFinds(detail::Mutation::SCAN_IGNORE_PUBLISHED, true);
}

} // namespace
