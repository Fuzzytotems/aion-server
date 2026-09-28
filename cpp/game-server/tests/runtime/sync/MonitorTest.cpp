// Monitor: Java synchronized / ReentrantLock semantics (design §3.4, §4.1), eventual fairness, watchdog records (§4.3).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "SyncTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

namespace {

struct GameObject {
	Monitor& monitor() const noexcept { return monitor_; }
	mutable Monitor monitor_;
};

uintptr_t idOf(const Monitor& monitor) {
	return reinterpret_cast<uintptr_t>(&monitor);
}

} // namespace

TEST(MonitorTest, ReentrancyHoldCountAndOwnership) {
	HangGuard guard(10s);
	Monitor monitor;
	EXPECT_FALSE(monitor.isLocked());
	EXPECT_EQ(monitor.getHoldCount(), 0);
	monitor.lock();
	EXPECT_TRUE(monitor.tryLock());
	EXPECT_TRUE(monitor.tryLock(1ms));
	EXPECT_EQ(monitor.getHoldCount(), 3);

	std::thread([&] {
		EXPECT_TRUE(monitor.isLocked());
		EXPECT_FALSE(monitor.isHeldByCurrentThread());
		EXPECT_EQ(monitor.getHoldCount(), 0);
		EXPECT_FALSE(monitor.tryLock());
		EXPECT_THROW(monitor.unlock(), IllegalMonitorStateException); // Java: unlock by a non-owner
	}).join();

	monitor.unlock();
	monitor.unlock();
	EXPECT_TRUE(monitor.isHeldByCurrentThread());
	monitor.unlock();
	EXPECT_FALSE(monitor.isLocked());
	EXPECT_THROW(monitor.unlock(), IllegalMonitorStateException);

	std::thread([&] {
		EXPECT_TRUE(monitor.tryLock());
		monitor.unlock();
	}).join();
}

TEST(MonitorTest, SynchronizedReleasesOnException) {
	GameObject object;
	EXPECT_THROW(
		{
			SYNCHRONIZED(object) {
				SYNCHRONIZED(object) {
					throw std::runtime_error("boom");
				}
			}
		},
		std::runtime_error);
	EXPECT_FALSE(object.monitor().isLocked());
	EXPECT_EQ(ThreadContext::current().heldLockCount.load(), 0u);
}

TEST(MonitorTest, HeldLockRecordsFollowAcquisitionsInAllBuilds) {
	Monitor first{AION_LOCK_CLASS(MonitorTest::first)};
	Monitor second{AION_LOCK_CLASS(MonitorTest::second)};
	ThreadContext& context = ThreadContext::current();
	uint32_t before = context.heldLockCount.load();
	SYNCHRONIZED(first) {
		SYNCHRONIZED(second) {
			SYNCHRONIZED(first) { // reentrant: not recorded again
				EXPECT_EQ(context.heldLockCount.load(), before + 2);
			}
			EXPECT_STREQ(context.heldLocks[before].lockClassName.load(), "MonitorTest::first");
			EXPECT_EQ(context.heldLocks[before + 1].lockId.load(), idOf(second));
			EXPECT_EQ(context.heldLocks[before + 1].rank.load(), 0);
		}
	}
	EXPECT_EQ(context.heldLockCount.load(), before);

	// non-LIFO release
	first.lock();
	second.lock();
	first.unlock();
	EXPECT_EQ(context.heldLockCount.load(), before + 1);
	EXPECT_EQ(context.heldLocks[before].lockId.load(), idOf(second));
	second.unlock();
	EXPECT_EQ(context.heldLockCount.load(), before);
}

TEST(MonitorTest, TimedTryLockTimesOutAndSucceedsWhenReleased) {
	HangGuard guard(20s);
	Monitor monitor;
	std::promise<void> locked;
	std::promise<void> release;
	std::thread holder([&] {
		monitor.lock();
		locked.set_value();
		release.get_future().wait();
		monitor.unlock();
	});
	locked.get_future().wait();

	auto start = std::chrono::steady_clock::now();
	EXPECT_FALSE(monitor.tryLock(50ms));
	auto elapsed = std::chrono::steady_clock::now() - start;
	EXPECT_GE(elapsed, 40ms);
	EXPECT_LT(elapsed, 5s);
	EXPECT_FALSE(monitor.tryLock(0ns));

	std::thread releaser([&] {
		std::this_thread::sleep_for(30ms);
		release.set_value();
	});
	EXPECT_TRUE(monitor.tryLock(10s));
	EXPECT_TRUE(monitor.isHeldByCurrentThread());
	monitor.unlock();
	holder.join();
	releaser.join();
}

TEST(MonitorTest, MutualExclusionUnderContention) {
	HangGuard guard(60s);
	Monitor monitor;
	int64_t counter = 0; // deliberately not atomic
	std::atomic<int> inside{0};
	std::atomic<bool> overlap{false};
	constexpr int THREADS = 8;
	constexpr int ITERATIONS = 20000;
	std::vector<std::thread> threads;
	for (int t = 0; t < THREADS; ++t)
		threads.emplace_back([&, t] {
			for (int i = 0; i < ITERATIONS; ++i) {
				if ((i + t) % 7 == 0) {
					if (!monitor.tryLock(std::chrono::microseconds(50)))
						continue;
					if (inside.fetch_add(1) != 0)
						overlap = true;
					++counter;
					inside.fetch_sub(1);
					monitor.unlock();
					continue;
				}
				SYNCHRONIZED(monitor) {
					if (inside.fetch_add(1) != 0)
						overlap = true;
					++counter;
					inside.fetch_sub(1);
				}
			}
		});
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_FALSE(overlap.load());
	EXPECT_GE(counter, int64_t{THREADS} * ITERATIONS * 6 / 7 - THREADS);
	EXPECT_LE(counter, int64_t{THREADS} * ITERATIONS);
	EXPECT_FALSE(monitor.isLocked());
}

TEST(MonitorTest, TimedOutWaitersDoNotLoseWakeups) {
	// many short timed waits racing with blocking lock(): a waiter that times out after consuming a wakeup must pass it on, otherwise a
	// blocked lock() would sleep forever (caught by the HangGuard)
	HangGuard guard(60s);
	Monitor monitor;
	std::atomic<int64_t> acquisitions{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; ++t)
		threads.emplace_back([&, t] {
			for (int i = 0; i < 3000; ++i) {
				if (t % 2 == 0) {
					if (monitor.tryLock(std::chrono::microseconds(t * 20 + 1))) {
						acquisitions.fetch_add(1);
						monitor.unlock();
					}
				} else {
					monitor.lock();
					acquisitions.fetch_add(1);
					monitor.unlock();
				}
			}
		});
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_GE(acquisitions.load(), 4 * 3000);
}

TEST(MonitorTest, EventualFairnessAgainstABargingThread) {
	HangGuard guard(60s);
	Monitor monitor;
	std::atomic<bool> stop{false};
	std::atomic<int64_t> bargerAcquisitions{0};
	std::thread barger([&] {
		while (!stop.load(std::memory_order_relaxed)) {
			monitor.lock();
			bargerAcquisitions.fetch_add(1, std::memory_order_relaxed);
			std::this_thread::sleep_for(std::chrono::microseconds(50)); // holds most of the time
			monitor.unlock();
		}
	});
	ASSERT_TRUE(waitUntil([&] { return bargerAcquisitions.load() > 10; }));
	for (int round = 0; round < 20; ++round) {
		auto start = std::chrono::steady_clock::now();
		monitor.lock();
		auto waited = std::chrono::steady_clock::now() - start;
		monitor.unlock();
		EXPECT_LT(waited, 2s) << "round " << round;
	}
	stop = true;
	barger.join();
}

TEST(MonitorTest, BlockedWaiterIsVisibleToTheWatchdogRecords) {
	HangGuard guard(20s);
	Monitor monitor{AION_LOCK_CLASS(MonitorTest::visible)};
	std::promise<void> release;
	std::promise<uint64_t> holderId;
	std::thread holder([&] {
		holderId.set_value(ThreadContext::current().threadId());
		SYNCHRONIZED(monitor) {
			release.get_future().wait();
		}
	});
	uint64_t holderThread = holderId.get_future().get();
	ASSERT_TRUE(waitUntil([&] { return threadHoldsLock(holderThread, idOf(monitor)); }));

	std::atomic<uint64_t> waiterThread{0};
	std::thread waiter([&] {
		waiterThread = ThreadContext::current().threadId();
		SYNCHRONIZED(monitor) {
		}
	});
	ASSERT_TRUE(waitUntil([&] { return waiterThread.load() != 0 && threadWaitsFor(waiterThread.load(), idOf(monitor)); }));
	withThread(waiterThread.load(), [&](const ThreadContext& context) {
		ThreadContext::WaitSnapshot wait = context.wait();
		EXPECT_STREQ(wait.lockClassName, "MonitorTest::visible");
		EXPECT_GT(wait.waitStartNanos, 0);
	});
	release.set_value();
	holder.join();
	waiter.join();
}

TEST(MonitorTest, LeafMutexForbidsMonitorsInAllBuilds) {
	RankedMutex<LockRank::IDFACTORY> leaf;
	Monitor monitor;
	std::scoped_lock lock(leaf);
	EXPECT_THROW(monitor.lock(), IllegalStateException);
	EXPECT_THROW((void)monitor.tryLock(), IllegalStateException);
	EXPECT_THROW((void)monitor.tryLock(1ms), IllegalStateException);
	EXPECT_FALSE(monitor.isLocked());
}

TEST(MonitorTest, DynamicLockClassOfObjectMonitors) {
	GameObject object;
	MonitorHandle handle = monitorOf(object);
	EXPECT_EQ(&handle.lockClass, &LockClass::ofType(typeid(GameObject)));
	SYNCHRONIZED(object) {
		ThreadContext& context = ThreadContext::current();
		EXPECT_EQ(std::string(context.heldLocks[context.heldLockCount.load() - 1].lockClassName.load()), handle.lockClass.name());
	}
	Monitor named{AION_LOCK_CLASS(Owner::field)};
	std::unique_lock lock(named); // Lockable
	EXPECT_TRUE(named.isHeldByCurrentThread());
}

namespace {

struct CachedTypeObject {
	Monitor& monitor() const noexcept { return monitor_; }
	mutable Monitor monitor_;
};

template <int N>
struct ManyTypesObject {
	Monitor& monitor() const noexcept { return monitor_; }
	mutable Monitor monitor_;
};

} // namespace

// Review finding: SYNCHRONIZED on an object monitor resolved its dynamic lock class under the lock class registry's global mutex on every
// acquisition. After the first resolution of a type the lookup must be lock-free.
TEST(MonitorTest, ObjectMonitorClassesResolveWithoutTheRegistryMutex) {
	HangGuard guard(30s);
	CachedTypeObject object;
	const LockClass* first = &monitorOf(object).lockClass;
	std::atomic<bool> resolved{false};
	std::atomic<bool> sameClass{false};
	std::thread other;
	bool resolvedWhileLocked = false;
	detail::testing::withLockClassRegistryLocked([&] {
		other = std::thread([&] {
			for (int i = 0; i < 1000; ++i) {
				SYNCHRONIZED(object) {
				}
			}
			sameClass = &monitorOf(object).lockClass == first;
			resolved = true;
		});
		resolvedWhileLocked = waitUntil([&] { return resolved.load(); }, 5000ms);
	});
	other.join();
	EXPECT_TRUE(resolvedWhileLocked) << "a cached object monitor class needed the registry mutex";
	EXPECT_TRUE(sameClass.load());
}

TEST(MonitorTest, ConcurrentFirstResolutionsOfManyTypesAgree) {
	HangGuard guard(30s);
	constexpr int THREADS = 8;
	std::vector<std::vector<const LockClass*>> seen(THREADS);
	std::vector<std::thread> threads;
	std::promise<void> go;
	std::shared_future<void> start = go.get_future().share();
	for (int t = 0; t < THREADS; ++t) {
		threads.emplace_back([&, t] {
			start.wait();
			seen[t] = {&monitorOf(ManyTypesObject<1>{}).lockClass, &monitorOf(ManyTypesObject<2>{}).lockClass,
				&monitorOf(ManyTypesObject<3>{}).lockClass, &monitorOf(ManyTypesObject<4>{}).lockClass, &monitorOf(ManyTypesObject<5>{}).lockClass,
				&monitorOf(ManyTypesObject<6>{}).lockClass, &monitorOf(ManyTypesObject<7>{}).lockClass, &monitorOf(ManyTypesObject<8>{}).lockClass};
		});
	}
	go.set_value();
	for (std::thread& thread : threads)
		thread.join();
	for (int t = 1; t < THREADS; ++t)
		EXPECT_EQ(seen[t], seen[0]);
	for (size_t i = 0; i < seen[0].size(); ++i)
		for (size_t j = i + 1; j < seen[0].size(); ++j)
			EXPECT_NE(seen[0][i], seen[0][j]) << "distinct types have distinct classes";
	EXPECT_NE(seen[0][0]->name().find("ManyTypesObject"), std::string::npos) << seen[0][0]->name();
}
