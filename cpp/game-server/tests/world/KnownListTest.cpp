// World chunk (P4-10): KnownList visibility with synthetic objects in the test map (WorldTestSupport.h), and the pair lock of addPair and the
// pair removals (runtime-architecture.md §5.3, RR-17): a pair added concurrently with a despawn, a range removal or another addPair is never left
// one-sided.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "WorldTestSupport.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/exceptions/AlreadySpawnedException.h"
#include "aion/gameserver/world/exceptions/DuplicateAionObjectException.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::world::test {
namespace {

using model::gameobjects::VisibleObject;

class KnownListTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
	}

	/** the objects spawned in Poeta's main instance */
	static int32_t poetaObjectCount() {
		int32_t count = 0;
		World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance()->forEachObject([&count](VisibleObject&) { ++count; });
		return count;
	}

	/** creates, stores and positions an object in Poeta's main instance (not spawned) */
	static runtime::Ref<TestObject> place(int32_t objectId, float x, float y, float visibleDistance = 95) {
		runtime::Ref<TestObject> object = VisibleObject::create<TestObject>(objectId, POETA, visibleDistance);
		EXPECT_TRUE(World::getInstance().setPosition(*object, POETA, x, y, 10, int8_t{0}));
		World::getInstance().storeObject(*object);
		return object;
	}
};

TEST_F(KnownListTest, ObjectsInRangeKnowAndSeeEachOther) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	World& world = World::getInstance();
	runtime::Ref<TestObject> a = place(1001, 500, 500);
	runtime::Ref<TestObject> b = place(1002, 550, 500);   // 50 m: in range, neighbour region
	runtime::Ref<TestObject> far = place(1003, 900, 900); // out of range

	world.spawn(runtime::Ptr<VisibleObject>(*a));
	EXPECT_TRUE(a->isSpawned());
	EXPECT_THROW(world.spawn(runtime::Ptr<VisibleObject>(*a)), exceptions::AlreadySpawnedException);
	EXPECT_THROW(world.storeObject(*a), exceptions::DuplicateAionObjectException);
	world.spawn(runtime::Ptr<VisibleObject>(*far));
	world.spawn(runtime::Ptr<VisibleObject>(*b)); // b's update finds a in its neighbour regions (addPair)

	EXPECT_TRUE(a->getKnownList().knows(*b));
	EXPECT_TRUE(b->getKnownList().knows(*a)) << "knowing is a two-way relation";
	EXPECT_TRUE(a->getKnownList().sees(*b)) << "VisibleObject.canSee: any object";
	EXPECT_FALSE(a->getKnownList().knows(*far));
	EXPECT_FALSE(far->getKnownList().knows(*b));
	EXPECT_EQ(a->recorder().seen.load(), 1);
	EXPECT_EQ(b->recorder().seen.load(), 1);
	EXPECT_EQ(a->getKnownList().getObject(1002).get(), b.get());
	EXPECT_FALSE(a->getKnownList().getPlayer(1002)) << "not a player";
	EXPECT_EQ(a->getKnownList().stream().size(), 1u);
	int32_t visited = 0;
	a->getKnownList().forEachObject([&visited](VisibleObject&) { ++visited; });
	EXPECT_EQ(visited, 1);
	EXPECT_EQ(poetaObjectCount(), 3);

	// b moves away (200 m): its known list update forgets a on both sides
	world.updatePosition(*b, 750, 500, 10, int8_t{0});
	EXPECT_EQ(b->getPosition()->getMapRegion()->getRegionId(), 5003);
	EXPECT_FALSE(a->getKnownList().knows(*b));
	EXPECT_FALSE(b->getKnownList().knows(*a));
	EXPECT_EQ(a->recorder().notKnown.load(), 1);
	EXPECT_EQ(b->recorder().notKnown.load(), 1);
	EXPECT_EQ(a->recorder().notSeen.load(), 1) << "a saw b";

	// back in range; then a despawns and clears both sides
	world.updatePosition(*b, 520, 510, 10, int8_t{0});
	EXPECT_TRUE(a->getKnownList().knows(*b));
	world.despawn(*a);
	EXPECT_FALSE(a->isSpawned());
	EXPECT_FALSE(b->getKnownList().knows(*a));
	EXPECT_FALSE(a->getKnownList().knows(*b));
	EXPECT_FALSE(a->getPosition()->getMapRegion()->getObjects().containsKey(1001));
	EXPECT_EQ(poetaObjectCount(), 2);

	// removeObject despawns and deletes
	EXPECT_TRUE(world.removeObject(*b));
	EXPECT_FALSE(world.isInWorld(1002));
	EXPECT_FALSE(b->isSpawned());
	EXPECT_FALSE(world.removeObject(*b)) << "not in world any more";
	EXPECT_TRUE(world.removeObject(*a));
	EXPECT_TRUE(world.removeObject(*far));
	EXPECT_EQ(poetaObjectCount(), 0);
}

/**
 * m5a-plan.md W-07: a controller that throws out of see() / notSee() / notKnow() must not break the known list (Java swallows the exception and
 * logs it), but every swallowed exception is counted, so the scenario gate can fail a run in which a notification threw. The counter is the
 * only signal besides the log line: Java's `log.error("", ex)` writes an empty message.
 */
TEST_F(KnownListTest, ThrowingNotificationsAreSwallowedLoggedAndCounted) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	World& world = World::getInstance();
	knownlist::KnownList::resetNotifyFailureCountForTests();

	std::ostringstream captured;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(captured);
	sink->set_pattern("%v");
	commons::logging::LoggerFactory::configure("com.aionemu.gameserver.world.knownlist.KnownList", {.sinks = {sink}, .additive = false});

	runtime::Ref<TestObject> a = place(2101, 300, 900);
	runtime::Ref<TestObject> b = place(2102, 310, 900); // 10 m: in range
	a->recorder().hook = [](RecordingController::Event, VisibleObject&) { throw runtime::IllegalStateException("notification failed"); };

	world.spawn(runtime::Ptr<VisibleObject>(*a));
	world.spawn(runtime::Ptr<VisibleObject>(*b)); // b's update adds the pair: a.see(b) throws

	EXPECT_EQ(knownlist::KnownList::notifyFailureCount(), 1u) << "see() threw once";
	EXPECT_TRUE(a->getKnownList().knows(*b)) << "the entry stays, as in Java";
	EXPECT_TRUE(b->getKnownList().knows(*a)) << "the throwing side does not break the other side";
	EXPECT_EQ(b->recorder().seen.load(), 1) << "b's own notification is unaffected";

	// b moves out of range: a.notSee(b) and a.notKnow(b) throw as well
	world.updatePosition(*b, 700, 900, 10, int8_t{0});
	EXPECT_EQ(knownlist::KnownList::notifyFailureCount(), 3u) << "notSee() and notKnow() threw";
	EXPECT_FALSE(a->getKnownList().knows(*b)) << "the removal happened before the notification";
	EXPECT_FALSE(b->getKnownList().knows(*a));

	commons::logging::LoggerFactory::removeConfig("com.aionemu.gameserver.world.knownlist.KnownList");
	EXPECT_NE(captured.str().find("notification failed"), std::string::npos)
		<< "Java logs the exception with an empty message: " << captured.str();

	a->recorder().hook = nullptr;
	world.removeObject(*a);
	world.removeObject(*b);
	knownlist::KnownList::resetNotifyFailureCountForTests();
}

/**
 * The other end of W-07: what a gate reads is not `notifyFailureCount()` at the moment of the failure but the number
 * `CheckOutput::writeSummary` puts into `m5a_summary.txt` as `knownListNotifyFailures` (CheckOutput.cpp:257) - taken by the ShutdownHook, on a
 * thread that never touched a known list, when everything that failed is long destroyed. §5.7 Q8 of the scenario gate then asserts that number is
 * 0 (and only a gate run reaches it: a startup without a client builds no known list at all, `NpcKnownList::update` clears while the map region
 * is inactive). That assertion is worth nothing unless the count survives the objects and crosses threads, so this test reads it the way the
 * report does. A per-instance or thread-local counter passes ThrowingNotificationsAreSwallowedLoggedAndCounted above and fails here.
 * <p>
 * Measured end to end (stage 3 wave B): three throws injected into `NpcController::see` of a real server make the gate's run write
 * `knownListNotifyFailures 3` and fail M5aScenarioTest's Q8 row; without them the same run writes 0 and passes.
 */
TEST_F(KnownListTest, TheReportedNotifyFailureCountIsTheOneAGateReads) {
	knownlist::KnownList::resetNotifyFailureCountForTests();
	// the three swallowed exceptions are logged with Java's empty message; the test does not need them on the console
	std::ostringstream captured;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(captured);
	commons::logging::LoggerFactory::configure("com.aionemu.gameserver.world.knownlist.KnownList", {.sinks = {sink}, .additive = false});

	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		World& world = World::getInstance();
		runtime::Ref<TestObject> a = place(2301, 300, 1000);
		runtime::Ref<TestObject> b = place(2302, 310, 1000); // 10 m: in range
		a->recorder().hook = [](RecordingController::Event, VisibleObject&) { throw runtime::IllegalStateException("notification failed"); };
		world.spawn(runtime::Ptr<VisibleObject>(*a));
		world.spawn(runtime::Ptr<VisibleObject>(*b));       // a.see(b) throws
		world.updatePosition(*b, 700, 1000, 10, int8_t{0}); // a.notSee(b) and a.notKnow(b) throw
		a->recorder().hook = nullptr;
		world.removeObject(*a);
		world.removeObject(*b);
	}
	runtime::Reclaimer::getInstance().drain(); // both objects are destroyed, as they are when the ShutdownHook writes the summary

	// the ShutdownHook's read: another thread, no known list in sight
	uint64_t reported = 0;
	std::thread reader([&reported] { reported = knownlist::KnownList::notifyFailureCount(); });
	reader.join();
	commons::logging::LoggerFactory::removeConfig("com.aionemu.gameserver.world.knownlist.KnownList");
	EXPECT_EQ(reported, 3u) << "the count a report row carries is one per notifySee/notifyNotSee/notifyNotKnow catch of the whole process, "
	                           "counted where the exception is swallowed and readable when the objects are gone";
	knownlist::KnownList::resetNotifyFailureCountForTests();
	EXPECT_EQ(knownlist::KnownList::notifyFailureCount(), 0u) << "a clean run reports 0, which is what the gate asserts";
}

TEST_F(KnownListTest, ClearWithoutNotifyDropsTheEntriesSilently) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	World& world = World::getInstance();
	runtime::Ref<TestObject> a = place(2001, 100, 900);
	runtime::Ref<TestObject> b = place(2002, 110, 900);
	world.spawn(runtime::Ptr<VisibleObject>(*a));
	world.spawn(runtime::Ptr<VisibleObject>(*b));
	ASSERT_TRUE(a->getKnownList().knows(*b));
	EXPECT_TRUE(a->getKnownList().clearWithoutNotify());
	EXPECT_FALSE(a->getKnownList().knows(*b));
	EXPECT_TRUE(b->getKnownList().knows(*a)) << "the zombie breaker cuts only the own edges";
	EXPECT_EQ(a->recorder().notKnown.load(), 0);
	EXPECT_FALSE(a->getKnownList().clearWithoutNotify()) << "idempotent";
	world.removeObject(*a);
	world.removeObject(*b);
}

/**
 * addPair handshake: thread 1 spawns and despawns `mover` again and again, thread 2 updates the known list of `watcher` (always spawned, in
 * range). Without the handshake, watcher's update can add the pair after the despawn cleared mover's known list, leaving watcher knowing a
 * despawned object. After every despawn completes and the updater has stopped, nobody knows the mover.
 */
TEST_F(KnownListTest, AddPairHandshakeWithConcurrentDespawn) {
	runtime::Ref<TestObject> watcher;
	runtime::Ref<TestObject> mover;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		World::getInstance(); // created inside a task scope
		watcher = place(3001, 800, 100);
		mover = place(3002, 810, 100);
		World::getInstance().spawn(runtime::Ptr<VisibleObject>(*watcher));
	}
	World& world = World::getInstance();
	std::atomic<bool> stop{false};
	std::atomic<int64_t> updates{0};
	std::thread updater([&] {
		while (!stop.load()) {
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			watcher->updateKnownlist();
			mover->updateKnownlist();
			updates.fetch_add(1);
		}
	});
	constexpr int32_t ROUNDS = 10000;
	for (int32_t round = 0; round < ROUNDS; round++) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		world.spawn(runtime::Ptr<VisibleObject>(*mover));
		if (round % 2 == 0)
			std::this_thread::yield();
		world.despawn(*mover, model::animations::ObjectDeleteAnimation::NONE);
	}
	stop.store(true);
	updater.join();
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		int32_t leftBehind = 0;
		if (watcher->getKnownList().knows(*mover))
			leftBehind++;
		if (mover->getKnownList().knows(*watcher))
			leftBehind++;
		EXPECT_EQ(leftBehind, 0) << "a despawned object stayed in a known list";
		EXPECT_GT(updates.load(), 0);
		world.removeObject(*watcher);
		world.removeObject(*mover);
	}
	std::cout << "addPair stress: " << ROUNDS << " spawn/despawn rounds, " << updates.load() << " concurrent known list updates\n";
	runtime::Reclaimer::getInstance().drain();
}

TEST_F(KnownListTest, MovingOutsideTheRegionsLogsTheCoordinatesLikeJava) {
	// World.java:194: slf4j formats the float coordinates with String.valueOf(float): "X 5000.0", not "X 5000"
	std::ostringstream stream;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	sink->set_pattern("%v");
	commons::logging::LoggerFactory::configure("com.aionemu.gameserver.world.World", {.sinks = {sink}, .additive = false});
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	World& world = World::getInstance();
	runtime::Ref<TestObject> object = place(6001, 700, 700);
	world.spawn(runtime::Ptr<VisibleObject>(*object));
	world.updatePosition(*object, 5000, 700.5f, 10, int8_t{0});
	commons::logging::LoggerFactory::removeConfig("com.aionemu.gameserver.world.World");
	EXPECT_NE(stream.str().find("doesn't exist at coordinates: Map 210010000, X 5000.0, Y 700.5, Z 10.0"), std::string::npos) << stream.str();
	EXPECT_FLOAT_EQ(object->getX(), 700.0f) << "the position is not changed";
	world.removeObject(*object);
}

/** The role of the current test thread, read by the controller hooks of RollbackNeverRemovesTheEdgesOfAConcurrentAddPair. */
thread_local int32_t gateRole = 0;

/** A one-shot rendezvous: the first pass() blocks until release() (at most 10 s); the test thread waits for it with a timeout. */
class Gate {
public:
	void pass() {
		std::unique_lock lock(mutex);
		if (reached)
			return;
		reached = true;
		condition.notify_all();
		condition.wait_for(lock, std::chrono::seconds(10), [this] { return released; });
	}

	bool waitReached(std::chrono::milliseconds timeout) {
		std::unique_lock lock(mutex);
		return condition.wait_for(lock, timeout, [this] { return reached; });
	}

	void release() {
		std::scoped_lock lock(mutex);
		released = true;
		condition.notify_all();
	}

private:
	std::mutex mutex;
	std::condition_variable condition;
	bool reached = false;
	bool released = false;
};

/**
 * Review finding (P4-10, addPair rollback): the interleaving that left a one-sided edge when addPair rolled back with two unconditional dels.
 * T1 updates watcher's known list and adds the pair (watcher, mover); T1 stops in watcher's see(mover) (gate 1). D despawns mover and stops in
 * mover's notKnow(watcher) (gate 2), after the despawn clear removed mover's edge. T1 goes on: the old handshake saw the despawn, deleted
 * watcher's edge and stopped in watcher's notKnow(mover) (gate 3). D finishes the despawn and respawns mover, whose update adds a fresh pair.
 * Then T1 finished its rollback by deleting mover's fresh edge: watcher knew mover, mover did not know watcher. With the pair lock the rollback
 * and the clear are atomic per pair and gate 3 is never reached; the respawned pair is known on both sides.
 */
TEST_F(KnownListTest, RollbackNeverRemovesTheEdgesOfAConcurrentAddPair) {
	constexpr int32_t T1 = 1;
	constexpr int32_t D = 2;
	runtime::Ref<TestObject> watcher;
	runtime::Ref<TestObject> mover;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		World::getInstance();
		watcher = place(5001, 600, 300);
		mover = place(5002, 610, 300);
		World::getInstance().spawn(runtime::Ptr<VisibleObject>(*mover)); // nobody else is near: mover knows nobody
	}
	World& world = World::getInstance();
	Gate watcherSeesMover;
	Gate moverForgetsWatcher;
	Gate watcherForgetsMover;
	watcher->recorder().hook = [&](RecordingController::Event event, VisibleObject& object) {
		if (gateRole == T1 && object.getObjectId() == 5002 && event == RecordingController::Event::SEE)
			watcherSeesMover.pass();
		if (gateRole == T1 && object.getObjectId() == 5002 && event == RecordingController::Event::NOT_KNOW)
			watcherForgetsMover.pass();
	};
	mover->recorder().hook = [&](RecordingController::Event event, VisibleObject& object) {
		if (gateRole == D && object.getObjectId() == 5001 && event == RecordingController::Event::NOT_KNOW)
			moverForgetsWatcher.pass();
	};
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		// watcher is spawned without its own update: World.spawn would add the pair on this thread
		watcher->getPosition()->setIsSpawned(true);
		watcher->getPosition()->getMapRegion()->getParent().addObject(*watcher);
		watcher->getPosition()->getMapRegion()->add(*watcher);
	}

	std::thread t1([&] {
		gateRole = T1;
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		watcher->updateKnownlist();
	});
	ASSERT_TRUE(watcherSeesMover.waitReached(std::chrono::seconds(5))) << "T1 added the pair";
	std::thread d([&] {
		gateRole = D;
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			world.despawn(*mover, model::animations::ObjectDeleteAnimation::NONE);
		}
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		world.spawn(runtime::Ptr<VisibleObject>(*mover));
	});
	bool despawnCleared = moverForgetsWatcher.waitReached(std::chrono::seconds(5));
	watcherSeesMover.release();
	bool rollbackStopped = watcherForgetsMover.waitReached(std::chrono::seconds(1));
	moverForgetsWatcher.release();
	d.join();
	watcherForgetsMover.release();
	t1.join();
	watcher->recorder().hook = nullptr;
	mover->recorder().hook = nullptr;

	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_TRUE(despawnCleared) << "the despawn clear removed mover's edge";
	EXPECT_FALSE(rollbackStopped) << "T1's handshake passed before the despawn: nothing to roll back";
	EXPECT_TRUE(mover->isSpawned());
	EXPECT_EQ(watcher->getKnownList().knows(*mover), mover->getKnownList().knows(*watcher)) << "one-sided known list edge";
	EXPECT_TRUE(watcher->getKnownList().knows(*mover)) << "the respawn added the pair again";
	EXPECT_TRUE(mover->getKnownList().knows(*watcher));
	world.removeObject(*watcher);
	world.removeObject(*mover);
}

/**
 * The two-way relation under concurrent pair adds from both sides, range removals and despawn/respawn (review finding: an addPair rollback
 * could delete an edge that a concurrent addPair had just inserted, leaving a one-sided edge). Three objects are despawned and respawned,
 * three move in and out of range, and three threads update random known lists. After every thread has stopped, each relation is two-way and
 * nobody knows a despawned object.
 */
TEST_F(KnownListTest, ConcurrentPairAddsRemovalsAndDespawnsKeepTheRelationTwoWay) {
	constexpr int32_t COUNT = 6;
	constexpr int32_t CHURNED = 3; // objects 0..2 despawn and respawn, objects 3..5 move
	std::vector<runtime::Ref<TestObject>> objects;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		World::getInstance();
		for (int32_t i = 0; i < COUNT; i++) {
			objects.push_back(place(4001 + i, 300.0f + 10.0f * static_cast<float>(i), 600));
			World::getInstance().spawn(runtime::Ptr<VisibleObject>(*objects.back()));
		}
	}
	World& world = World::getInstance();
	std::atomic<bool> stop{false};
	std::atomic<int64_t> updates{0};
	std::atomic<int64_t> churns{0};
	std::atomic<int64_t> moves{0};
	std::vector<std::thread> threads;
	for (uint32_t t = 0; t < 3; t++) {
		threads.emplace_back([&, t] {
			uint32_t seed = 0x9E3779B9u * (t + 1);
			while (!stop.load()) {
				runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
				seed = seed * 1664525u + 1013904223u;
				objects[(seed >> 16) % COUNT]->updateKnownlist();
				updates.fetch_add(1);
			}
		});
	}
	threads.emplace_back([&] {
		uint32_t seed = 12345;
		while (!stop.load()) {
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			seed = seed * 1664525u + 1013904223u;
			TestObject& object = *objects[(seed >> 16) % CHURNED];
			if (object.isSpawned())
				world.despawn(object, model::animations::ObjectDeleteAnimation::NONE);
			else
				world.spawn(runtime::Ptr<VisibleObject>(object));
			churns.fetch_add(1);
		}
	});
	threads.emplace_back([&] {
		uint32_t seed = 54321;
		while (!stop.load()) {
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			seed = seed * 1664525u + 1013904223u;
			TestObject& object = *objects[CHURNED + (seed >> 16) % (COUNT - CHURNED)];
			// near: 330..360 (in range of everyone), far: 430..460 (out of range of 0..2, at the edge of 3..5)
			float x = ((seed >> 8) & 1) != 0 ? 330.0f + static_cast<float>((seed >> 20) % 31) : 430.0f + static_cast<float>((seed >> 20) % 31);
			world.updatePosition(object, x, 600, 10, int8_t{0});
			moves.fetch_add(1);
		}
	});
	std::this_thread::sleep_for(std::chrono::seconds(3));
	stop.store(true);
	for (std::thread& thread : threads)
		thread.join();
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		int32_t oneSided = 0;
		int32_t knowsDespawned = 0;
		for (int32_t i = 0; i < COUNT; i++) {
			for (int32_t j = 0; j < COUNT; j++) {
				if (i == j)
					continue;
				bool iKnowsJ = objects[i]->getKnownList().knows(*objects[j]);
				if (i < j && iKnowsJ != objects[j]->getKnownList().knows(*objects[i]))
					oneSided++;
				if (iKnowsJ && !objects[j]->isSpawned())
					knowsDespawned++;
			}
		}
		EXPECT_EQ(oneSided, 0) << "a known list relation is one-sided";
		EXPECT_EQ(knowsDespawned, 0) << "a despawned object stayed in a known list";
		EXPECT_GT(updates.load(), 0);
		EXPECT_GT(churns.load(), 0);
		EXPECT_GT(moves.load(), 0);
		for (runtime::Ref<TestObject>& object : objects)
			world.removeObject(*object);
	}
	std::cout << "pair symmetry stress: " << updates.load() << " updates, " << churns.load() << " despawns/respawns, " << moves.load()
			  << " moves\n";
	runtime::Reclaimer::getInstance().drain();
}

} // namespace
} // namespace aion::gameserver::world::test
