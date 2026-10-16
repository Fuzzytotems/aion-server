// LeakCensus holder probe (LeakCensus::setHolderProbe): the census counts the references of a leaked object but cannot say whose they are, so
// the game layer installs a probe (world::WorldLeakProbe) that names them. A census check that reports new leaks posts the probe once to the
// instant pool for at most MAX_PROBED_LEAKS_PER_PASS of them, oldest removal first, pinned on them; the others are skipped, and so are the new
// leaks of a check less than a minute after the last probe or while it has not finished (a systematic leak costs one pass over the world a
// minute, not one per object or per check). A zero-threshold check (CheckOutput's final census) posts none: its caller re-reads the counts.
// Leak of the 2026-09-24 client session: an Npc at refcount 1 with no pinning task that the log could not attribute (docs/deviations/P4-10.md).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using namespace std::chrono;

/** One leak the probe was called with (the deterministic executor runs it on the test thread). */
struct ProbedObject {
	std::string className;
	int32_t objectId;
	uint32_t refCountDuringProbe;
};

/** the leaks of each probe call */
std::vector<std::vector<ProbedObject>>& probeCalls() {
	static std::vector<std::vector<ProbedObject>> calls;
	return calls;
}

void recordingProbe(std::span<const LeakCensus::ProbedLeak> leaks) {
	std::vector<ProbedObject>& call = probeCalls().emplace_back();
	for (const LeakCensus::ProbedLeak& leak : leaks)
		call.push_back(ProbedObject{leak.className, leak.objectId, leak.object->refCount()});
}

void throwingProbe(std::span<const LeakCensus::ProbedLeak>) {
	throw std::runtime_error("probe failed");
}

std::vector<int32_t> idsOf(const std::vector<ProbedObject>& call) {
	std::vector<int32_t> ids;
	for (const ProbedObject& object : call)
		ids.push_back(object.objectId);
	return ids;
}

class LeakCensusHolderProbeTest : public DeterministicServicesTest {
protected:
	void SetUp() override {
		DeterministicServicesTest::SetUp();
		probeCalls().clear();
		census.configure(LeakCensus::Config{}); // reports after 10 minutes, zombie breaker after 30
		census.install();
	}

	void TearDown() override {
		census.setHolderProbe(nullptr);
		probeCalls().clear();
		DeterministicServicesTest::TearDown();
	}

	/** lets simulated time pass, running the tasks due, and runs a scan (the census hook) */
	void later(milliseconds duration) {
		pass(duration);
		reclaim();
	}

	/** lets simulated time pass WITHOUT running tasks (a posted probe stays pending) and runs a scan */
	void laterWithoutTasks(milliseconds duration) {
		clock.advance(duration);
		reclaim();
	}

	LeakCensus& census = LeakCensus::getInstance();
};

TEST_F(LeakCensusHolderProbeTest, AReportedLeakGetsTheProbeOncePinnedOnTheObject) {
	census.setHolderProbe(&recordingProbe);
	Ref<TestObject> npc = TestObject::create(25582);
	census.onRemovedFromWorld(*npc, "Npc", 25582);
	reclaim();
	later(minutes(9));
	EXPECT_EQ(executor->pendingTaskCount(), 0u) << "no report, no probe";

	later(minutes(1)); // the report posts the probe
	EXPECT_EQ(executor->pendingTaskCount(), 1u);
	EXPECT_EQ(npc->refCount(), 2u) << "the test's Ref and the probe task's pin";
	EXPECT_TRUE(probeCalls().empty()) << "posted, not run inside the census hook";
	executor->runReady();
	ASSERT_EQ(probeCalls().size(), 1u);
	ASSERT_EQ(probeCalls()[0].size(), 1u);
	EXPECT_EQ(probeCalls()[0][0].className, "Npc");
	EXPECT_EQ(probeCalls()[0][0].objectId, 25582);
	EXPECT_EQ(probeCalls()[0][0].refCountDuringProbe, 2u) << "the probe gets the object itself, kept alive by the pin while it runs";

	later(minutes(15));
	executor->runReady();
	EXPECT_EQ(probeCalls().size(), 1u) << "once per reported object";
	reclaim();
	EXPECT_EQ(npc->refCount(), 1u) << "the pin is released when the probe ran";
}

TEST_F(LeakCensusHolderProbeTest, WithoutAProbeTheReportPostsNothing) {
	Ref<TestObject> npc = TestObject::create(7);
	census.onRemovedFromWorld(*npc, "Npc", 7);
	reclaim();
	later(minutes(10));
	EXPECT_EQ(census.getLeaks().size(), 1u);
	EXPECT_EQ(executor->pendingTaskCount(), 0u);
}

TEST_F(LeakCensusHolderProbeTest, AThrowingProbeIsLogged) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	census.setHolderProbe(&throwingProbe);
	Ref<TestObject> npc = TestObject::create(9);
	census.onRemovedFromWorld(*npc, "Gatherable", 9);
	reclaim();
	later(minutes(10));
	executor->runReady();
	EXPECT_TRUE(capture.contains("Leak census: the holder probe failed for Gatherable (object id 9)")) << capture.str();
}

TEST_F(LeakCensusHolderProbeTest, OneCheckProbesTheFourOldestOfItsNewLeaksInOneTaskAndLogsTheOthers) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	census.setHolderProbe(&recordingProbe);
	// six objects removed one second apart; their ids fall as the removal times rise, so "oldest first" is not the id order
	std::vector<Ref<TestObject>> objects;
	for (int32_t i = 0; i < 6; ++i) {
		objects.push_back(TestObject::create(106 - i));
		census.onRemovedFromWorld(*objects.back(), "Npc", 106 - i);
		clock.advance(seconds(1));
	}
	reclaim();
	later(minutes(10)); // one check reports all six
	ASSERT_EQ(census.getLeaks().size(), 6u);
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "one probe task for the whole check";
	EXPECT_TRUE(capture.contains("Leak census: the holder probe runs for 4 of 6 new leaks, 2 skipped")) << capture.str();
	for (size_t i = 0; i < objects.size(); ++i)
		EXPECT_EQ(objects[i]->refCount(), i < 4 ? 2u : 1u) << "object " << objects[i]->objectId << ": only the probed leaks are pinned";
	executor->runReady();
	ASSERT_EQ(probeCalls().size(), 1u);
	EXPECT_EQ(idsOf(probeCalls()[0]), (std::vector<int32_t>{106, 105, 104, 103})) << "the four oldest removals, oldest first";
	for (const ProbedObject& probed : probeCalls()[0])
		EXPECT_EQ(probed.refCountDuringProbe, 2u) << probed.objectId << " is pinned while the probe runs";

	later(minutes(20));
	executor->runReady();
	EXPECT_EQ(probeCalls().size(), 1u) << "the two skipped leaks are not probed later: each leak is reported once";
	reclaim();
	for (const Ref<TestObject>& object : objects)
		EXPECT_EQ(object->refCount(), 1u) << "the pins are released when the probe ran";
}

TEST_F(LeakCensusHolderProbeTest, TheNewLeaksOfACheckWhileTheProbeIsPendingAreSkippedAndLogged) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	census.setHolderProbe(&recordingProbe);
	Ref<TestObject> first = TestObject::create(1);
	census.onRemovedFromWorld(*first, "Npc", 1);
	clock.advance(minutes(2));
	Ref<TestObject> second = TestObject::create(2);
	census.onRemovedFromWorld(*second, "Npc", 2);
	reclaim();
	laterWithoutTasks(minutes(8)); // first reported, its probe posted and left pending
	ASSERT_EQ(executor->pendingTaskCount(), 1u);
	laterWithoutTasks(minutes(2)); // second reported while the probe is pending
	EXPECT_EQ(census.getLeaks().size(), 2u);
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "no second probe task";
	EXPECT_TRUE(capture.contains("Leak census: 1 new leak(s) not probed for holders: the previous holder probe has not finished")) << capture.str();
	EXPECT_EQ(second->refCount(), 1u) << "the skipped leak is not pinned";
	executor->runReady();
	ASSERT_EQ(probeCalls().size(), 1u);
	EXPECT_EQ(idsOf(probeCalls()[0]), std::vector<int32_t>{1});

	// the finished probe let the next check probe again
	Ref<TestObject> third = TestObject::create(3);
	census.onRemovedFromWorld(*third, "Npc", 3);
	reclaim();
	later(minutes(10));
	executor->runReady();
	ASSERT_EQ(probeCalls().size(), 2u);
	EXPECT_EQ(idsOf(probeCalls()[1]), std::vector<int32_t>{3});
}

TEST_F(LeakCensusHolderProbeTest, ADroppedProbeTaskLetsTheNextCheckProbeAgain) {
	census.setHolderProbe(&recordingProbe);
	Ref<TestObject> first = TestObject::create(1);
	census.onRemovedFromWorld(*first, "Npc", 1);
	reclaim();
	laterWithoutTasks(minutes(10));
	ASSERT_EQ(executor->pendingTaskCount(), 1u);
	// the backend goes away with the probe pending (shutdown, or a test replacing the pools): the task never runs
	ThreadPoolManager::installBackend(nullptr);
	auto backend = std::make_unique<DeterministicExecutor>(clock, 43);
	executor = backend.get();
	ThreadPoolManager::installBackend(std::move(backend));
	reclaim();
	EXPECT_EQ(first->refCount(), 1u) << "the dropped task released its pin";
	Ref<TestObject> second = TestObject::create(2);
	census.onRemovedFromWorld(*second, "Npc", 2);
	reclaim();
	later(minutes(10));
	executor->runReady();
	ASSERT_EQ(probeCalls().size(), 1u) << "the dropped probe does not block the next one";
	EXPECT_EQ(idsOf(probeCalls()[0]), std::vector<int32_t>{2});
}

TEST_F(LeakCensusHolderProbeTest, TheProbeIsPostedAtMostOnceAMinuteAndTheCensusLogsWhatItSkips) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	census.setHolderProbe(&recordingProbe);
	Ref<TestObject> first = TestObject::create(1);
	census.onRemovedFromWorld(*first, "Npc", 1);
	clock.advance(seconds(30));
	Ref<TestObject> second = TestObject::create(2);
	census.onRemovedFromWorld(*second, "Npc", 2);
	clock.advance(seconds(40));
	Ref<TestObject> third = TestObject::create(3);
	census.onRemovedFromWorld(*third, "Npc", 3);
	reclaim();
	later(minutes(10) - seconds(70)); // first reported and probed
	executor->runReady();
	ASSERT_EQ(probeCalls().size(), 1u);
	EXPECT_EQ(idsOf(probeCalls()[0]), std::vector<int32_t>{1});

	later(seconds(30)); // second reported 30 s after the probe was posted; that probe has finished
	EXPECT_EQ(census.getLeaks().size(), 2u);
	EXPECT_EQ(executor->pendingTaskCount(), 0u) << "no second probe within the minute";
	EXPECT_EQ(second->refCount(), 1u) << "the skipped leak is not pinned";
	EXPECT_TRUE(capture.contains("Leak census: 1 new leak(s) not probed for holders: the holder probe runs at most once a minute")) << capture.str();

	later(seconds(40)); // third reported 70 s after the first probe was posted
	executor->runReady();
	std::vector<std::vector<int32_t>> calls;
	for (const std::vector<ProbedObject>& call : probeCalls())
		calls.push_back(idsOf(call));
	EXPECT_EQ(calls, (std::vector<std::vector<int32_t>>{{1}, {3}})) << "the third is probed, the skipped second never (each leak is reported once)";
}

TEST_F(LeakCensusHolderProbeTest, AZeroThresholdCheckReportsTheLeakWithoutAProbe) {
	// CheckOutput::runFinalCensus and runBreakerPass report with censusAfter 0 and re-read the counts until two checks agree: a probe's pin
	// would hold up the count of an object whose references are still being released (CheckOutputTest.FinalCensusWithAHolderProbe...)
	LeakCensus::Config config;
	config.censusAfter = milliseconds(0);
	config.checkInterval = milliseconds(0);
	config.zombieBreakerEnabled = false;
	census.configure(config);
	census.setHolderProbe(&recordingProbe);
	Ref<TestObject> player = TestObject::create(61);
	census.onRemovedFromWorld(*player, "Player", 61);
	reclaim();
	reclaim();
	ASSERT_EQ(census.getLeaks().size(), 1u) << "the zero threshold reports the object at the next check";
	EXPECT_EQ(executor->pendingTaskCount(), 0u) << "no probe task for the leaks of a zero-threshold check";
	EXPECT_EQ(player->refCount(), 1u) << "nothing pins the reported object";
	executor->runReady();
	EXPECT_TRUE(probeCalls().empty());
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
