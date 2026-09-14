// CleanerQueue and CleanerDrain (design §6, RR-2): push order, the drain posted by the Reclaimer post-scan hook after destructors pushed ids,
// the Java Cleaner body with a RespawnService-style deferral, exceptions, at most one pending drain, shutdown, concurrent producers.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

/** ids seen by the recording action, in call order (test thread or drain task; accessed under a mutex) */
struct Seen {
	static inline std::mutex mutex;
	static inline std::vector<std::pair<int32_t, std::string>> calls;
	static inline std::atomic<bool> inTaskScope{true};

	static void record(int32_t id, const char* className) {
		if (!TaskScope::active())
			inTaskScope = false;
		std::scoped_lock lock(mutex);
		calls.emplace_back(id, className != nullptr ? className : "");
	}
	static std::vector<std::pair<int32_t, std::string>> take() {
		std::scoped_lock lock(mutex);
		return std::exchange(calls, {});
	}
};

/**
 * RespawnService stand-in (RespawnService.java:95-100,201-226): an id registered for a pending respawn is not released by the Cleaner but
 * handed to the respawn under the service's monitor; unregister releases it later.
 */
struct FakeRespawnService {
	static inline Monitor monitor;
	static inline std::set<int32_t> pendingRespawns;
	static inline std::set<int32_t> autoReleaseIds;

	/** Java setAutoReleaseId: true if a respawn took over the id */
	static bool setAutoReleaseId(int32_t objectId) {
		bool takenOver = false;
		SYNCHRONIZED(monitor) {
			takenOver = pendingRespawns.contains(objectId);
			if (takenOver)
				autoReleaseIds.insert(objectId);
		}
		return takenOver;
	}
	/** Java unregister: the respawn ends and releases the id it took over */
	static void unregister(int32_t objectId) {
		bool release = false;
		SYNCHRONIZED(monitor) {
			pendingRespawns.erase(objectId);
			release = autoReleaseIds.erase(objectId) > 0;
		}
		if (release)
			IDFactory::getInstance().releaseId(objectId);
	}
	/** the Java Cleaner body of AionObject */
	static void cleanerBody(int32_t objectId, const char* className) {
		if (!setAutoReleaseId(objectId))
			IDFactory::getInstance().releaseId(objectId, className);
	}
};

class CleanerQueueTest : public DeterministicServicesTest {
protected:
	void SetUp() override {
		DeterministicServicesTest::SetUp();
		(void)Seen::take();
		Seen::inTaskScope = true;
	}
};

TEST_F(CleanerQueueTest, DrainNowRunsTheActionInPushOrder) {
	CleanerQueue::setCleanerAction(&Seen::record);
	for (int32_t id = 1; id <= 100; ++id)
		CleanerQueue::push(id, id % 2 == 0 ? "Npc" : nullptr);
	EXPECT_EQ(CleanerQueue::size(), 100u);
	EXPECT_FALSE(CleanerQueue::isEmpty());
	EXPECT_EQ(CleanerQueue::drainNow(), 100u);
	EXPECT_TRUE(CleanerQueue::isEmpty());
	std::vector<std::pair<int32_t, std::string>> calls = Seen::take();
	ASSERT_EQ(calls.size(), 100u);
	for (int32_t i = 0; i < 100; ++i) {
		EXPECT_EQ(calls[static_cast<size_t>(i)].first, i + 1);
		EXPECT_EQ(calls[static_cast<size_t>(i)].second, (i + 1) % 2 == 0 ? "Npc" : "");
	}
	EXPECT_TRUE(Seen::inTaskScope.load());
	EXPECT_EQ(CleanerQueue::drainNow(), 0u);
}

TEST_F(CleanerQueueTest, TheDefaultActionReleasesTheIdInTheIDFactory) {
	IDFactory& factory = IDFactory::getInstance();
	int32_t id = factory.nextId();
	CleanerQueue::push(id, "Npc");
	EXPECT_EQ(CleanerQueue::drainNow(), 1u);
	EXPECT_EQ(factory.getQuarantinedCount(), 1);
}

TEST_F(CleanerQueueTest, DestroyedObjectsAreDrainedByAPostedInstantTask) {
	CleanerQueue::install();
	CleanerQueue::install(); // idempotent
	EXPECT_TRUE(CleanerQueue::isInstalled());
	CleanerQueue::setCleanerAction(&Seen::record);
	uint64_t postedBefore = CleanerQueue::getPostedDrainCount();

	std::vector<Ref<TestObject>> objects;
	for (int32_t id = 11; id <= 13; ++id)
		objects.push_back(TestObject::create(id, true));
	objects.clear(); // last releases: queued for reclamation
	EXPECT_TRUE(Seen::take().empty());

	Reclaimer::getInstance().reclaimNow(); // destroys them (destructors push 11, 12, 13), then the hook posts one drain
	EXPECT_EQ(CleanerQueue::size(), 3u);
	EXPECT_EQ(CleanerQueue::getPostedDrainCount(), postedBefore + 1);
	std::vector<FutureRef> pending = executor->pendingTasks();
	ASSERT_EQ(pending.size(), 1u);
	EXPECT_EQ(pending[0]->getPool(), PoolKind::INSTANT);
	pending.clear();
	EXPECT_TRUE(Seen::take().empty()); // destructors never run the action themselves

	executor->runReady();
	std::vector<std::pair<int32_t, std::string>> calls = Seen::take();
	std::ranges::sort(calls); // the Reclaimer chooses the destruction order; push order is checked by DrainNowRunsTheActionInPushOrder
	EXPECT_EQ(calls, (std::vector<std::pair<int32_t, std::string>>{{11, "TestObject"}, {12, "TestObject"}, {13, "TestObject"}}));
	EXPECT_TRUE(CleanerQueue::isEmpty());
	EXPECT_TRUE(Seen::inTaskScope.load());
}

TEST_F(CleanerQueueTest, AtMostOneDrainIsPendingAtATime) {
	CleanerQueue::install();
	CleanerQueue::setCleanerAction(&Seen::record);
	uint64_t postedBefore = CleanerQueue::getPostedDrainCount();
	CleanerQueue::push(1);
	Reclaimer::getInstance().reclaimNow();
	CleanerQueue::push(2);
	Reclaimer::getInstance().reclaimNow();
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(CleanerQueue::getPostedDrainCount(), postedBefore + 1);
	executor->runReady(); // the drain takes both ids; the scans after the task find the queue empty
	EXPECT_EQ(Seen::take().size(), 2u);
	CleanerQueue::push(3);
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(CleanerQueue::getPostedDrainCount(), postedBefore + 2);
	executor->runReady();
	EXPECT_EQ(Seen::take().size(), 1u);
	CleanerQueue::uninstall();
	CleanerQueue::push(4);
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(CleanerQueue::getPostedDrainCount(), postedBefore + 2); // no hook
	EXPECT_EQ(CleanerQueue::drainNow(), 1u);
}

TEST_F(CleanerQueueTest, JavaCleanerBodyWithRespawnDeferral) {
	IDFactory& factory = IDFactory::getInstance();
	CleanerQueue::install();
	CleanerQueue::setCleanerAction(&FakeRespawnService::cleanerBody);
	FakeRespawnService::pendingRespawns.clear();
	FakeRespawnService::autoReleaseIds.clear();

	int32_t respawning = factory.nextId();
	int32_t plain = factory.nextId();
	FakeRespawnService::pendingRespawns.insert(respawning); // RespawnService.scheduleRespawnTask registered the id
	{
		Ref<TestObject> first = TestObject::create(respawning, true);
		Ref<TestObject> second = TestObject::create(plain, true);
	}
	executor->runReady();
	reclaim();
	executor->runReady();
	EXPECT_EQ(factory.getQuarantinedCount(), 1); // only the plain id was released by the Cleaner
	EXPECT_TRUE(FakeRespawnService::autoReleaseIds.contains(respawning));

	FakeRespawnService::unregister(respawning); // the respawn ran: now the id is released
	EXPECT_EQ(factory.getQuarantinedCount(), 2);
	EXPECT_EQ(factory.getUsedCount(), 3); // id 0 plus both ids in quarantine
}

TEST_F(CleanerQueueTest, AnExceptionForOneIdDoesNotStopTheDrain) {
	LogCapture capture("com.aionemu.gameserver.model.gameobjects.AionObject");
	CleanerQueue::setCleanerAction([](int32_t id, const char* className) {
		if (id == 2)
			throw IllegalStateException("cleaner failure");
		Seen::record(id, className);
	});
	for (int32_t id = 1; id <= 3; ++id)
		CleanerQueue::push(id);
	EXPECT_EQ(CleanerQueue::drainNow(), 3u);
	EXPECT_EQ(Seen::take().size(), 2u);
	EXPECT_TRUE(capture.contains("Cleaner action failed for object ID 2"));
	EXPECT_TRUE(capture.contains("cleaner failure"));
}

TEST_F(CleanerQueueTest, IdsPushedByTheActionWaitForTheNextDrain) {
	CleanerQueue::setCleanerAction([](int32_t id, const char* className) {
		Seen::record(id, className);
		if (id < 3)
			CleanerQueue::push(id + 1);
	});
	CleanerQueue::push(1);
	EXPECT_EQ(CleanerQueue::drainNow(), 1u);
	EXPECT_EQ(CleanerQueue::drainNow(), 1u);
	EXPECT_EQ(CleanerQueue::drainNow(), 1u);
	EXPECT_EQ(CleanerQueue::drainNow(), 0u);
	EXPECT_EQ(Seen::take().size(), 3u);
}

TEST_F(CleanerQueueTest, NothingIsPostedAfterThreadPoolManagerShutdown) {
	CleanerQueue::install();
	CleanerQueue::setCleanerAction(&Seen::record);
	ThreadPoolManager::getInstance().shutdown();
	uint64_t postedBefore = CleanerQueue::getPostedDrainCount();
	CleanerQueue::push(5);
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(CleanerQueue::getPostedDrainCount(), postedBefore);
	EXPECT_EQ(CleanerQueue::drainNow(), 1u); // design §11 step 6: the final drain after ThreadPoolManager::shutdown
	EXPECT_EQ(Seen::take().size(), 1u);
}

// Stress finding: the post-scan hooks (CleanerDrain posting, LeakCensus) called ThreadPoolManager::getInstance() and lazily created the default
// pools after a test teardown had removed the backend. They must never create pools: the queued ids wait for drainNow().
TEST_F(CleanerQueueTest, KernelHooksNeverCreateTheDefaultPools) {
	CleanerQueue::install();
	LeakCensus::getInstance().install();
	CleanerQueue::setCleanerAction(&Seen::record);
	ThreadPoolManager::installBackend(nullptr); // e.g. a test teardown
	uint64_t postedBefore = CleanerQueue::getPostedDrainCount();
	Ref<TestObject> object = TestObject::create(7, true);
	LeakCensus::getInstance().onRemovedFromWorld(*object, "Npc", 7);
	object.reset();
	reclaim(); // destroys the object (pushes its id) and runs the census and Cleaner hooks without a backend
	EXPECT_EQ(ThreadPoolManager::installedBackend(), nullptr) << "a hook created the default pools";
	EXPECT_NO_THROW(ThreadPoolManager::configure(ThreadPoolManager::Config{})) << "a hook created the default pools";
	EXPECT_EQ(CleanerQueue::getPostedDrainCount(), postedBefore);
	EXPECT_EQ(CleanerQueue::drainNow(), 1u);
	EXPECT_EQ(Seen::take().size(), 1u);
}

TEST_F(CleanerQueueTest, ConcurrentProducersAndADrainerLoseNothingAndKeepPerThreadOrder) {
	constexpr int32_t THREADS = 8;
	constexpr int32_t PER_THREAD = 20'000;
	static std::atomic<int64_t> processed{0};
	static std::array<std::atomic<int32_t>, 8> lastSeen{};
	static std::atomic<int32_t> orderViolations{0};
	processed = 0;
	orderViolations = 0;
	for (auto& last : lastSeen)
		last = -1;
	CleanerQueue::setCleanerAction([](int32_t id, const char*) {
		int32_t thread = id / PER_THREAD;
		int32_t sequence = id % PER_THREAD;
		if (lastSeen[static_cast<size_t>(thread)].exchange(sequence) >= sequence)
			++orderViolations;
		++processed;
	});
	std::atomic<bool> producing{true};
	std::thread drainer([&] {
		while (producing.load())
			(void)CleanerQueue::drainNow();
		(void)CleanerQueue::drainNow();
	});
	std::vector<std::thread> producers;
	for (int32_t t = 0; t < THREADS; ++t)
		producers.emplace_back([t] {
			for (int32_t i = 0; i < PER_THREAD; ++i)
				CleanerQueue::push(t * PER_THREAD + i);
		});
	for (std::thread& producer : producers)
		producer.join();
	producing = false;
	drainer.join();
	EXPECT_EQ(processed.load(), int64_t{THREADS} * PER_THREAD);
	EXPECT_EQ(orderViolations.load(), 0);
	EXPECT_EQ(CleanerQueue::size(), 0u);
	EXPECT_EQ(CleanerQueue::getDroppedCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
