// LeakCensus and the zombie breaker (design §5.3, §5.4, D7) on synthetic object graphs: tracking until destruction (also when the object dies
// before the census saw its event), leak reports with pinning tasks, re-adds, the zombie breaker cutting a reference cycle, objects without a
// breaker, stale periodic pins, concurrent producers against the Reclaimer thread (ASan), and the //debug reports.
// Stage 3 (m5a-plan.md §10): the self-retaining cycle a throwing periodic body leaves behind (AbstractInteractionTask's shape), and the
// zero-threshold shutdown pass of CheckOutput::runBreakerPass, which makes the gate's "zombieCuts 0" and "no stale pin" rows reachable in a
// run of a few minutes.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/Introspection.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using namespace std::chrono;

/** A world object with one retaining edge that the zombie breaker may cut ("peer": zombie-safe in cycles.toml terms). */
class Node final : public RefCounted, public ZombieBreakable {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Node> create(int32_t objectId) { return makeRef<Node>(objectId); }
	static inline std::atomic<int32_t> live{0};
	const int32_t objectId;
	Field<Ref<Node>> peer;

	std::vector<const char*> breakKnownEdges() override {
		if (peer.exchange(nullptr))
			return {"peer"};
		return {};
	}

protected:
	explicit Node(int32_t objectId) : objectId(objectId) { live.fetch_add(1); }
	~Node() override { live.fetch_sub(1); }
};

/**
 * The shape of every "stop() cancels the periodic task" resolution in cycles.toml, AbstractInteractionTask (AbstractInteractionTask.java:68-90)
 * being the one the M5a gate can reach: the object holds the Future, the scheduled body holds a Ref back to the object (Java: the anonymous
 * Runnable's outer instance) and the task pins the world object it works on. The body ends the task by calling stop(); a body that throws
 * before that never gets there. Java behaves exactly like this port: ThreadPoolManager.scheduleAtFixedRate wraps the Runnable in
 * RunnableWrapper(catchAndLogThrowables = true) (ThreadPoolManager.java:60-62), so the exception is logged inside the body and the task is
 * re-armed with its captures - Future does the same (logExceptions, Future.h:89).
 */
class InteractionTask final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<InteractionTask> create(Node& owner) { return makeRef<InteractionTask>(owner); }
	static inline std::atomic<int32_t> live{0};
	std::atomic<int32_t> runs{0};
	std::atomic<bool> throwing{true};

	/** AbstractInteractionTask.start() */
	void start() {
		Ref<InteractionTask> self(*this);
		task = ThreadPoolManager::getInstance().scheduleAtFixedRate(
			Pin(owner.get()), [self] { self->runInteraction(); }, 60'000, 60'000);
	}

	/** AbstractInteractionTask$1.run() -> runInteraction(): onInteraction() throws before the stop() that would cut the cycle */
	void runInteraction() {
		runs.fetch_add(1);
		if (throwing.load())
			throw std::runtime_error("onInteraction failed");
		stop();
	}

	/** AbstractInteractionTask.stop() */
	void stop() {
		FutureRef current = task;
		if (current && !current->isCancelled()) {
			current->cancel(false);
			task = nullptr;
		}
	}

	bool isInProgress() const { return task && !task->isCancelled(); }

protected:
	explicit InteractionTask(Node& ownerValue) : owner(ownerValue) { live.fetch_add(1); }
	~InteractionTask() override { live.fetch_sub(1); }

private:
	const Ref<Node> owner;
	/** Java `private Future<?> task` (a Field in the ported class; the deterministic executor runs this test on one thread) */
	FutureRef task;
};

class LeakCensusTest : public DeterministicServicesTest {
protected:
	void SetUp() override {
		DeterministicServicesTest::SetUp();
		LeakCensus::Config config;
		config.censusAfter = minutes(10);
		config.zombieBreakAfter = minutes(30);
		config.checkInterval = seconds(1);
		config.stalePinAfter = minutes(10);
		config.stalePinCheckInterval = minutes(1);
		census.configure(config);
		census.install();
	}

	/** lets simulated time pass without tasks and runs a scan (the census hook) */
	void later(milliseconds duration) {
		pass(duration);
		reclaim();
	}

	LeakCensus& census = LeakCensus::getInstance();
};

TEST_F(LeakCensusTest, RemovedObjectsAreTrackedUntilDestroyed) {
	EXPECT_TRUE(census.isInstalled());
	Ref<TestObject> npc = TestObject::create(1);
	census.onRemovedFromWorld(*npc, "Npc", 1);
	EXPECT_EQ(census.trackedCount(), 0u); // not moved yet
	reclaim();
	EXPECT_EQ(census.trackedCount(), 1u);
	npc.reset();
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
	later(minutes(60));
	EXPECT_TRUE(census.getLeaks().empty());
}

TEST_F(LeakCensusTest, ObjectsDestroyedBeforeTheCensusSawTheirEventAreErasedFirst) {
	int32_t liveBefore = TestObject::live.load();
	std::vector<Ref<TestObject>> survivors;
	{
		std::vector<Ref<TestObject>> objects;
		for (int32_t id = 0; id < 2000; ++id)
			objects.push_back(TestObject::create(id));
		for (const Ref<TestObject>& object : objects)
			census.onRemovedFromWorld(*object, "Npc", object->objectId);
		for (size_t i = 0; i < objects.size(); i += 100)
			survivors.push_back(objects[i]);
	}
	// one scan destroys 1980 objects: the destroy observer moves the queued events and erases each entry before its memory is freed
	reclaim();
	EXPECT_EQ(TestObject::live.load(), liveBefore + 20);
	EXPECT_EQ(census.trackedCount(), 20u);
	later(minutes(11)); // the hook reads the reference counts of the survivors only (ASan: no access to freed objects)
	EXPECT_EQ(census.getLeaks().size(), 20u);
	survivors.clear();
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
	EXPECT_TRUE(census.getLeaks().empty());
}

TEST_F(LeakCensusTest, LeaksAreReportedOnceWithTheirPinningTasks) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	Ref<TestObject> npc = TestObject::create(42);
	const int32_t scheduleLine = __LINE__ + 1;
	FutureRef task = ThreadPoolManager::getInstance().schedule(Pin(npc.get()), [] {}, duration_cast<milliseconds>(hours(2)).count());
	census.onRemovedFromWorld(*npc, "Npc", 42);
	reclaim();
	later(minutes(9));
	EXPECT_TRUE(census.getLeaks().empty());
	later(minutes(1));
	std::vector<LeakCensus::LeakReport> leaks = census.getLeaks();
	ASSERT_EQ(leaks.size(), 1u);
	EXPECT_EQ(leaks[0].className, "Npc");
	EXPECT_EQ(leaks[0].objectId, 42);
	EXPECT_EQ(leaks[0].refCount, 2u); // the test's Ref and the task's pin
	EXPECT_EQ(leaks[0].removedFor, seconds(600));
	ASSERT_EQ(leaks[0].pinningTasks.size(), 1u);
	EXPECT_EQ(leaks[0].pinningTasks[0].where.line(), static_cast<uint_least32_t>(scheduleLine));
	EXPECT_TRUE(leaks[0].cutEdges.empty());
	EXPECT_EQ(capture.count("Leak census: Npc (object id 42) is still alive 10 minutes after its removal from the world, refcount 2"), 1u);
	EXPECT_TRUE(capture.contains("LeakCensusTest.cpp:" + std::to_string(scheduleLine)));

	later(minutes(5));
	EXPECT_EQ(capture.count("Leak census:"), 1u);
	std::vector<std::string> lines = introspection::debugLeaks();
	ASSERT_GE(lines.size(), 3u);
	EXPECT_NE(lines[0].find("1 leak(s)"), std::string::npos) << lines[0];
	EXPECT_NE(lines[1].find("Npc (id 42): refcount 2, removed 15 min ago"), std::string::npos) << lines[1];
	EXPECT_NE(lines[2].find("pinned by scheduled task LeakCensusTest.cpp:" + std::to_string(scheduleLine)), std::string::npos) << lines[2];
	std::vector<std::string> pinning = introspection::tasksFor(*npc);
	ASSERT_EQ(pinning.size(), 2u);
	EXPECT_NE(pinning[1].find("LeakCensusTest.cpp:" + std::to_string(scheduleLine)), std::string::npos);

	task->cancel();
	task.reset();
	npc.reset();
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
}

TEST_F(LeakCensusTest, ReAddedObjectsLeaveTheCensus) {
	Ref<TestObject> npc = TestObject::create(5);
	census.onRemovedFromWorld(*npc, "Npc", 5);
	census.onAddedToWorld(*npc);
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
	census.onRemovedFromWorld(*npc, "Npc", 5);
	reclaim();
	EXPECT_EQ(census.trackedCount(), 1u);
	pass(minutes(5));
	census.onAddedToWorld(*npc);
	census.onRemovedFromWorld(*npc, "Npc", 5); // removed again: the removal time starts over
	later(minutes(6));
	EXPECT_TRUE(census.getLeaks().empty());
	later(minutes(4));
	EXPECT_EQ(census.getLeaks().size(), 1u);
}

TEST_F(LeakCensusTest, ZombieBreakerCutsACycleAndNamesTheEdges) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	int32_t liveBefore = Node::live.load();
	uint64_t cutsBefore = census.zombieCutCount(); // process-wide counter
	{
		Ref<Node> player = Node::create(1);
		Ref<Node> kisk = Node::create(2);
		{
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			player->peer = kisk;
			kisk->peer = player; // Kisk.java:174 style cycle without a breaker
		}
		census.onRemovedFromWorld(*player, "Player", 1);
		census.onRemovedFromWorld(*kisk, "Kisk", 2);
	}
	reclaim();
	EXPECT_EQ(Node::live.load(), liveBefore + 2); // leaked through the cycle
	later(minutes(29));
	EXPECT_EQ(census.zombieCutCount(), cutsBefore);
	EXPECT_EQ(census.getLeaks().size(), 2u);

	later(minutes(1)); // posts both breakers to the instant pool
	EXPECT_EQ(executor->pendingTaskCount(), 2u);
	EXPECT_EQ(census.zombieCutCount(), cutsBefore);
	executor->runReady();
	reclaim();
	EXPECT_EQ(census.zombieCutCount(), cutsBefore + 2);
	EXPECT_EQ(Node::live.load(), liveBefore);
	EXPECT_EQ(census.trackedCount(), 0u);
	EXPECT_TRUE(capture.contains("Zombie breaker: cut peer of Player (object id 1), removed from the world 30 minutes ago"));
	EXPECT_TRUE(capture.contains("Zombie breaker: cut peer of Kisk (object id 2)"));
	EXPECT_TRUE(capture.contains("a cycle breaker for this edge is missing"));
}

TEST_F(LeakCensusTest, ZombieBreakerRecordsCutsOfObjectsThatStayAlive) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	uint64_t cutsBefore = census.zombieCutCount();
	Ref<Node> held = Node::create(3);
	Ref<Node> other = Node::create(4);
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		held->peer = other;
	}
	Ref<TestObject> plain = TestObject::create(9);
	census.onRemovedFromWorld(*held, "Summon", 3);
	census.onRemovedFromWorld(*plain, "Gatherable", 9);
	reclaim();
	later(minutes(30));
	executor->runReady();
	reclaim();
	std::vector<LeakCensus::LeakReport> leaks = census.getLeaks();
	ASSERT_EQ(leaks.size(), 2u);
	const LeakCensus::LeakReport& summon = leaks[0].className == "Summon" ? leaks[0] : leaks[1];
	ASSERT_EQ(summon.cutEdges.size(), 1u);
	EXPECT_STREQ(summon.cutEdges[0], "peer");
	EXPECT_TRUE(capture.contains("Gatherable (object id 9) is still alive 30 minutes after its removal from the world (refcount 1) and has no zombie breaker"));

	// idempotent breaker, posted only once per object
	later(minutes(30));
	executor->runReady();
	EXPECT_EQ(census.zombieCutCount(), cutsBefore + 1);
	EXPECT_EQ(capture.count("Zombie breaker:"), 2u);
	std::vector<std::string> lines = introspection::debugLeaks();
	EXPECT_NE(lines[0].find(std::format("2 leak(s), 2 tracked object(s), {} zombie breaker cut(s)", cutsBefore + 1)), std::string::npos) << lines[0];
}

TEST_F(LeakCensusTest, ZombieBreakerCanBeDisabledAndReportsBreakersThatCutNothing) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	uint64_t cutsBefore = census.zombieCutCount();
	LeakCensus::Config config = census.getConfig();
	config.zombieBreakerEnabled = false;
	census.configure(config);
	Ref<Node> lonely = Node::create(6);
	census.onRemovedFromWorld(*lonely, "Npc", 6);
	reclaim();
	later(minutes(31));
	EXPECT_EQ(executor->pendingTaskCount(), 0u);
	config.zombieBreakerEnabled = true;
	census.configure(config);
	later(seconds(1));
	executor->runReady();
	EXPECT_TRUE(capture.contains("Zombie breaker: Npc (object id 6) is still alive 31 minutes after its removal from the world and no known edge was cut"));
	EXPECT_EQ(census.zombieCutCount(), cutsBefore);
}

TEST_F(LeakCensusTest, PeriodicTasksPinningRemovedObjectsAreLoggedOnce) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	Ref<TestObject> npc = TestObject::create(77);
	FutureRef task = ThreadPoolManager::getInstance().scheduleAtFixedRate(Pin(npc.get()), [] {}, 60'000, 60'000);
	census.onRemovedFromWorld(*npc, "Npc", 77);
	npc.reset(); // only the periodic task keeps it alive
	pass(minutes(9)); // every run is followed by a scan
	EXPECT_EQ(capture.count("still pins"), 0u);
	pass(minutes(2));
	// the literal "stale pin" is what the M5a gate greps for (m5a-plan.md §5.7 Q8)
	EXPECT_EQ(capture.count("stale pin: periodic task scheduled task LeakCensusTest.cpp:"), 1u);
	EXPECT_TRUE(capture.contains("still pins Npc (object id 77), removed from the world 1")); // 10 or 11 minutes, depending on the scan
	EXPECT_TRUE(capture.contains("minutes ago (the task is not cancelled)"));
	pass(minutes(5));
	EXPECT_EQ(capture.count("still pins"), 1u);
	EXPECT_FALSE(task->isDone());
	task->cancel();
	task.reset();
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
}

// design §5.4: "a periodic task whose pins have ALL been removed from World for more than 10 minutes is logged once"
TEST_F(LeakCensusTest, PeriodicTasksAreStaleOnlyWhenAllTheirPinnedOwnersWereRemoved) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	Ref<TestObject> effector = TestObject::create(81);
	Ref<TestObject> effected = TestObject::create(82);
	FutureRef task = ThreadPoolManager::getInstance().scheduleAtFixedRate({effector.get(), effected.get()}, [] {}, 60'000, 60'000);
	auto owners = task->pinnedOwners();
	EXPECT_EQ(owners[0], effector.get());
	EXPECT_EQ(owners[1], effected.get());
	EXPECT_EQ(owners[2], nullptr);
	census.onRemovedFromWorld(*effector, "Npc", 81); // the effected creature stays in the world
	pass(minutes(15));
	EXPECT_EQ(capture.count("still pins"), 0u) << "one pinned owner is still in the world";
	census.onRemovedFromWorld(*effected, "Player", 82);
	pass(minutes(9));
	EXPECT_EQ(capture.count("still pins"), 0u);
	pass(minutes(2));
	EXPECT_EQ(capture.count("still pins"), 1u);
	EXPECT_TRUE(capture.contains("still pins Npc (object id 81), removed from the world 2")) << "both owners are named";
	EXPECT_TRUE(capture.contains("; Player (object id 82), removed from the world 1"));
	task->cancel();
	EXPECT_EQ(task->pinnedOwners()[0], nullptr) << "released with the Pin";
	task.reset();
	effector.reset();
	effected.reset();
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
}

// Review finding (stage 3): "stop() leaves a self-retaining Future cycle when the periodic body throws, and the cycles.toml resolution assumes
// stop() always runs". It does, and so does Java: the cycle AbstractInteractionTask -> Future -> body -> AbstractInteractionTask is cut by
// stop() alone, and a throwing onInteraction() never reaches it. The port is faithful (see InteractionTask above), so the answer is not a
// deviation but detection: the task holds its Pin, so the object it pins never leaves the census, and the stale-pin warning names the task.
TEST_F(LeakCensusTest, AThrowingPeriodicBodyLeavesTheSelfRetainingCycleThatStopWouldHaveCut) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	const int32_t nodesBefore = Node::live.load();
	const int32_t tasksBefore = InteractionTask::live.load();
	InteractionTask* leaked = nullptr;
	{
		Ref<Node> player = Node::create(11);
		Ref<InteractionTask> interaction = InteractionTask::create(*player);
		leaked = interaction.get();
		interaction->start();
		census.onRemovedFromWorld(*player, "Player", 11); // the character logs out while the interaction runs
	}
	reclaim();
	// nothing outside the task holds either object: only the periodic Future does
	ASSERT_EQ(Node::live.load(), nodesBefore + 1);
	ASSERT_EQ(InteractionTask::live.load(), tasksBefore + 1);

	pass(minutes(3)); // three runs, each throwing where Java's RunnableWrapper would log and re-arm too
	EXPECT_EQ(leaked->runs.load(), 3);
	EXPECT_TRUE(leaked->isInProgress()) << "a throwing body does not end a periodic task (Future.h:89, ThreadPoolManager.java:60-62)";
	EXPECT_EQ(Node::live.load(), nodesBefore + 1) << "the Player is still pinned by the task it started";

	// the shutdown pass of CheckOutput::runBreakerPass: at zero thresholds the leak is visible immediately instead of after 10/30 minutes
	LeakCensus::Config shutdown = census.getConfig();
	shutdown.censusAfter = milliseconds(0);
	shutdown.checkInterval = milliseconds(0);
	shutdown.zombieBreakerEnabled = true;
	shutdown.zombieBreakAfter = milliseconds(0);
	shutdown.stalePinAfter = milliseconds(0);
	shutdown.stalePinCheckInterval = milliseconds(0);
	census.configure(shutdown);
	Reclaimer::getInstance().reclaimNow();
	executor->runReady();
	std::vector<LeakCensus::LeakReport> leaks = census.getLeaks();
	ASSERT_EQ(leaks.size(), 1u);
	EXPECT_EQ(leaks[0].className, "Player");
	EXPECT_EQ(leaks[0].objectId, 11);
	ASSERT_EQ(leaks[0].pinningTasks.size(), 1u) << "the report names the pending task that holds it";
	EXPECT_TRUE(capture.contains("stale pin: periodic task")) << capture.str();
	EXPECT_TRUE(capture.contains("still pins Player (object id 11)")) << capture.str();
	EXPECT_TRUE(capture.contains("and no known edge was cut")) << "no cycle breaker can cut a Pin: only a cancel can";

	// the cycles.toml resolution, once stop() does run: the next run cancels the task, which releases the body (and with it the Ref back to the
	// InteractionTask) and the Pin. Nothing may dereference `leaked` afterwards - that is the point of the test.
	leaked->throwing.store(false);
	leaked = nullptr;
	pass(minutes(1));
	reclaim();
	EXPECT_EQ(Node::live.load(), nodesBefore);
	EXPECT_EQ(InteractionTask::live.load(), tasksBefore);
	EXPECT_EQ(census.trackedCount(), 0u);
}

// m5a-plan.md §5.7 Q8 and CheckOutput::runBreakerPass: with the configured thresholds (30 minutes for the zombie breaker, 10 for stale pins)
// neither row can fire in a one-to-three minute gate run, so both were decoration. The shutdown pass sets them to 0 and scans once, which
// reports what is left at the end of the run; on a clean run there is nothing in the table and neither fires.
TEST_F(LeakCensusTest, TheShutdownPassCutsZombiesAndNamesStalePinsWithoutWaitingOutTheThresholds) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	const uint64_t cutsBefore = census.zombieCutCount();
	const int32_t nodesBefore = Node::live.load();
	Ref<Node> player = Node::create(31);
	Ref<Node> kisk = Node::create(32);
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		player->peer = kisk;
		kisk->peer = player;
	}
	Ref<Node> pinned = Node::create(33);
	FutureRef periodic = ThreadPoolManager::getInstance().scheduleAtFixedRate(Pin(pinned.get()), [] {}, 60'000, 60'000);
	census.onRemovedFromWorld(*player, "Player", 31);
	census.onRemovedFromWorld(*kisk, "Kisk", 32);
	census.onRemovedFromWorld(*pinned, "Npc", 33);
	player.reset();
	kisk.reset();
	pinned.reset();
	reclaim();

	pass(seconds(90)); // the length of a gate run
	EXPECT_EQ(census.zombieCutCount(), cutsBefore) << "30 minutes are not reached in a run this short";
	EXPECT_EQ(capture.count("stale pin"), 0u) << "10 minutes are not reached either";
	EXPECT_TRUE(census.getLeaks().empty()) << "and the 10 minute census threshold is not reached";

	LeakCensus::Config shutdown = census.getConfig();
	shutdown.censusAfter = milliseconds(0);
	shutdown.checkInterval = milliseconds(0);
	shutdown.zombieBreakerEnabled = true;
	shutdown.zombieBreakAfter = milliseconds(0);
	shutdown.stalePinAfter = milliseconds(0);
	shutdown.stalePinCheckInterval = milliseconds(0);
	census.configure(shutdown);
	Reclaimer::getInstance().reclaimNow();
	executor->runReady(); // the breakers are posted to the instant pool
	Reclaimer::getInstance().reclaimNow();

	EXPECT_EQ(census.zombieCutCount(), cutsBefore + 2) << "the Player/Kisk cycle is cut in the same second it was found";
	EXPECT_TRUE(capture.contains("Zombie breaker: cut peer of Player (object id 31)")) << capture.str();
	EXPECT_TRUE(capture.contains("stale pin: periodic task")) << capture.str();
	EXPECT_TRUE(capture.contains("still pins Npc (object id 33)")) << capture.str();
	EXPECT_EQ(Node::live.load(), nodesBefore + 1) << "the cycle is gone; the pinned npc is still held by its task";

	periodic->cancel();
	periodic.reset();
	reclaim();
	EXPECT_EQ(Node::live.load(), nodesBefore);
	EXPECT_EQ(census.trackedCount(), 0u);
}

// A clean shutdown trips neither row: both only look at objects that left the world and are still referenced, which is what census.txt reports.
TEST_F(LeakCensusTest, TheShutdownPassIsSilentWhenNothingLeaked) {
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	const uint64_t cutsBefore = census.zombieCutCount();
	{
		Ref<Node> npc = Node::create(41);
		census.onRemovedFromWorld(*npc, "Npc", 41);
	}
	FutureRef periodic = ThreadPoolManager::getInstance().scheduleAtFixedRate([] {}, 60'000, 60'000); // a service task pinning nothing
	reclaim();

	LeakCensus::Config shutdown = census.getConfig();
	shutdown.censusAfter = milliseconds(0);
	shutdown.checkInterval = milliseconds(0);
	shutdown.zombieBreakerEnabled = true;
	shutdown.zombieBreakAfter = milliseconds(0);
	shutdown.stalePinAfter = milliseconds(0);
	shutdown.stalePinCheckInterval = milliseconds(0);
	census.configure(shutdown);
	Reclaimer::getInstance().reclaimNow();
	executor->runReady();
	Reclaimer::getInstance().reclaimNow();

	EXPECT_EQ(census.zombieCutCount(), cutsBefore);
	EXPECT_EQ(capture.count("stale pin"), 0u);
	EXPECT_TRUE(census.getLeaks().empty());
	EXPECT_EQ(census.trackedCount(), 0u);
	periodic->cancel();
}

// m5a-plan.md I-03/F-07: the final census of the check-output mode needs no LeakCensus::censusNow(). Configuring zero thresholds and running
// Reclaimer::reclaimNow() reports every removed object that is still referenced at once, without simulated time passing; objects already at
// count 0 are destroyed by the same scans and never reported.
TEST_F(LeakCensusTest, ZeroThresholdsAndReclaimNowReportOnDemand) {
	Ref<TestObject> kept = TestObject::create(21);
	Ref<TestObject> released = TestObject::create(22);
	census.onRemovedFromWorld(*kept, "Player", 21);
	census.onRemovedFromWorld(*released, "Item", 22);
	reclaim();
	EXPECT_TRUE(census.getLeaks().empty()); // the configured 10 minutes have not passed

	released.reset();
	LeakCensus::Config finalCensus = census.getConfig();
	finalCensus.censusAfter = milliseconds(0);
	finalCensus.checkInterval = milliseconds(0);
	finalCensus.zombieBreakerEnabled = false;
	census.configure(finalCensus);
	Reclaimer::getInstance().reclaimNow();
	Reclaimer::getInstance().reclaimNow();

	std::vector<LeakCensus::LeakReport> leaks = census.getLeaks();
	ASSERT_EQ(leaks.size(), 1u);
	EXPECT_EQ(leaks[0].className, "Player");
	EXPECT_EQ(leaks[0].objectId, 21);
	EXPECT_EQ(leaks[0].refCount, 1u);
	EXPECT_EQ(census.trackedCount(), 1u);

	kept.reset();
	reclaim();
	EXPECT_TRUE(census.getLeaks().empty());
	EXPECT_EQ(census.trackedCount(), 0u);
}

TEST_F(LeakCensusTest, UninstallForgetsEverything) {
	Ref<TestObject> npc = TestObject::create(8);
	census.onRemovedFromWorld(*npc, "Npc", 8);
	reclaim();
	EXPECT_EQ(census.trackedCount(), 1u);
	census.uninstall();
	EXPECT_FALSE(census.isInstalled());
	EXPECT_EQ(census.trackedCount(), 0u);
	census.onRemovedFromWorld(*npc, "Npc", 8); // no-op
	npc.reset();
	reclaim();
	EXPECT_EQ(census.trackedCount(), 0u);
	EXPECT_NE(introspection::debugLeaks()[0].find("not installed"), std::string::npos);
}

TEST_F(LeakCensusTest, ConcurrentWorldRemovalsAgainstTheReclaimerThread) {
	int32_t liveBefore = TestObject::live.load();
	Reclaimer::Config config = Reclaimer::getInstance().getConfig();
	Reclaimer::Config fast = config;
	fast.period = milliseconds(1);
	Reclaimer::getInstance().start(fast);
	constexpr int THREADS = 4;
	constexpr int ROUNDS = 3000;
	std::atomic<bool> running{true};
	std::thread reader([&] {
		while (running.load()) {
			(void)census.getLeaks();
			(void)census.trackedCount();
		}
	});
	std::vector<std::thread> threads;
	for (int t = 0; t < THREADS; ++t) {
		threads.emplace_back([&, t] {
			std::vector<Ref<TestObject>> keep;
			for (int round = 0; round < ROUNDS; ++round) {
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				Ref<TestObject> object = TestObject::create(t * ROUNDS + round);
				census.onRemovedFromWorld(*object, "Npc", object->objectId);
				if (round % 3 == 0)
					census.onAddedToWorld(*object);
				if (round % 5 == 0)
					census.onRemovedFromWorld(*object, "Npc", object->objectId);
				if (round % 7 == 0)
					keep.push_back(object);
				if (keep.size() > 20)
					keep.erase(keep.begin());
			}
		});
	}
	for (std::thread& thread : threads)
		thread.join();
	running = false;
	reader.join();
	Reclaimer::getInstance().stop();
	Reclaimer::getInstance().configure(config);
	reclaim();
	EXPECT_EQ(TestObject::live.load(), liveBefore);
	EXPECT_EQ(census.trackedCount(), 0u);
}

TEST_F(LeakCensusTest, DebugReportsProduceLines) {
	EXPECT_FALSE(introspection::debugTasks().empty());
	EXPECT_EQ(introspection::debugRefs().size(), 2u);
	std::vector<std::string> reclaimer = introspection::debugReclaimer();
	ASSERT_GE(reclaimer.size(), 3u);
	EXPECT_NE(reclaimer[0].find("epoch"), std::string::npos);
	(void)introspection::debugLocks();
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
