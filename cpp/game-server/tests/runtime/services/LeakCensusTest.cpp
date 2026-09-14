// LeakCensus and the zombie breaker (design §5.3, §5.4, D7) on synthetic object graphs: tracking until destruction (also when the object dies
// before the census saw its event), leak reports with pinning tasks, re-adds, the zombie breaker cutting a reference cycle, objects without a
// breaker, stale periodic pins, concurrent producers against the Reclaimer thread (ASan), and the //debug reports.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
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
	EXPECT_EQ(capture.count("Periodic task scheduled task LeakCensusTest.cpp:"), 1u);
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
