// Contract test of aion_gs_runtime_sync: exercises the public API shape. Behaviour is covered by MonitorTest, LockOrderValidatorTest,
// LocksTest and WatchdogTest.

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <thread>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/LockRank.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/runtime/sync/Semaphore.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"

using namespace aion::gameserver::runtime;

namespace {

/** an object with a Java monitor, like RefCounted/Immortal/shims */
struct Lockable {
	Monitor& monitor() const noexcept { return monitor_; }
	mutable Monitor monitor_;
};

struct TeamWithMemberLock {
	Monitor teamLock{AION_LOCK_CLASS(GeneralTeam::teamLock)};
};

int synchronizedReturn(Lockable& lockable, int value) {
	SYNCHRONIZED(lockable) {
		return value; // must compile without "not all control paths return a value"
	}
}

} // namespace

TEST(SyncContractTest, MonitorIsReentrantAndSmall) {
	EXPECT_LE(sizeof(Monitor), 32u);
	Monitor monitor;
	monitor.lock();
	monitor.lock();
	EXPECT_TRUE(monitor.isHeldByCurrentThread());
	EXPECT_EQ(monitor.getHoldCount(), 2);
	monitor.unlock();
	monitor.unlock();
	EXPECT_FALSE(monitor.isLocked());
	EXPECT_THROW(monitor.unlock(), IllegalMonitorStateException);
	{
		std::scoped_lock lock(monitor); // BasicLockable
		EXPECT_TRUE(monitor.isHeldByCurrentThread());
	}
}

TEST(SyncContractTest, SynchronizedMacroLocksObjectMonitorForTheBlock) {
	Lockable lockable;
	SYNCHRONIZED(lockable) {
		EXPECT_TRUE(lockable.monitor().isHeldByCurrentThread());
		SYNCHRONIZED(lockable) {
			EXPECT_EQ(lockable.monitor().getHoldCount(), 2);
		}
	}
	EXPECT_FALSE(lockable.monitor().isLocked());
	EXPECT_EQ(synchronizedReturn(lockable, 5), 5);
	EXPECT_FALSE(lockable.monitor().isLocked());
}

TEST(SyncContractTest, LockClassesAreInternedAndDerivedFromTypes) {
	TeamWithMemberLock team;
	EXPECT_EQ(&monitorOf(team.teamLock).lockClass, &LockClass::named("GeneralTeam::teamLock"));
	Lockable lockable;
	EXPECT_EQ(&monitorOf(lockable).lockClass, &LockClass::ofType(typeid(Lockable)));
	EXPECT_EQ(&LockClass::named("A::b"), &LockClass::named("A::b"));
}

TEST(SyncContractTest, MonitorContentionAcrossThreads) {
	Monitor monitor;
	int counter = 0;
	std::vector<std::jthread> threads;
	for (int t = 0; t < 4; ++t)
		threads.emplace_back([&] {
			for (int i = 0; i < 10000; ++i) {
				SYNCHRONIZED(monitor) {
					++counter;
				}
			}
		});
	threads.clear();
	EXPECT_EQ(counter, 40000);
}

TEST(SyncContractTest, LeafMutexRanksAndNoMonitorUnderLeaf) {
	RankedMutex<LockRank::SCHEDULER> scheduler;
	RankedMutex<LockRank::RECLAIMER> reclaimer;
	{
		std::scoped_lock outer(scheduler);
		std::scoped_lock inner(reclaimer); // 30 then 50: legal
		Monitor monitor;
		EXPECT_THROW(monitor.lock(), IllegalStateException); // C6: no Monitor under a leaf mutex
	}
	{
		std::scoped_lock outer(reclaimer);
		EXPECT_THROW(scheduler.lock(), IllegalStateException); // 50 then 30: rank violation
	}
}

TEST(SyncContractTest, StampedLockAndSemaphoreBasics) {
	StampedLock lock{AION_LOCK_CLASS(EffectController::lock)};
	int64_t read = lock.readLock();
	EXPECT_NE(read, 0);
	EXPECT_TRUE(lock.isReadLocked());
	lock.unlockRead(read);
	int64_t optimistic = lock.tryOptimisticRead();
	int64_t write = lock.writeLock();
	EXPECT_TRUE(lock.isWriteLocked());
	EXPECT_EQ(lock.tryReadLock(), 0);
	lock.unlockWrite(write);
	EXPECT_FALSE(lock.validate(optimistic));

	Semaphore semaphore(1);
	semaphore.acquireUninterruptibly();
	EXPECT_FALSE(semaphore.tryAcquire());
	semaphore.release();
	EXPECT_EQ(semaphore.availablePermits(), 1);
}

TEST(SyncContractTest, BlockingRegionIsVisibleToOtherThreads) {
	BlockingRegion region("Future.get");
	uint64_t self = ThreadContext::current().threadId();
	std::atomic<bool> seen{false};
	std::thread([&] {
		ThreadContext::forEach([&](const ThreadContext& context) {
			if (context.threadId() == self && context.blocking().active)
				seen = true;
		});
	}).join();
	EXPECT_TRUE(seen.load());
}

TEST(SyncContractTest, LockOrderValidatorReportsInversion) {
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	validator.clearReports(true);
	Monitor a{AION_LOCK_CLASS(Test::a)};
	Monitor b{AION_LOCK_CLASS(Test::b)};
	SYNCHRONIZED(a) {
		SYNCHRONIZED(b) {
		}
	}
	SYNCHRONIZED(b) {
		SYNCHRONIZED(a) {
		}
	}
	EXPECT_EQ(validator.reportCount(LockOrderValidator::ReportKind::CYCLE), validator.isEnabled() ? 1u : 0u);
	validator.clearReports(true);
	LockdepSuppression suppression("contract test");
}

TEST(SyncContractTest, WatchdogApiShape) {
	Watchdog& watchdog = Watchdog::getInstance();
	(void)ThreadContext::current(); // ctest runs each test in a fresh process: register this thread
	uint64_t listener = watchdog.addDumpListener([](const Watchdog::DumpReport&) {});
	uint64_t probe = watchdog.addProbe("contract", [](Watchdog&, const std::vector<Watchdog::ThreadSnapshot>&) {});
	EXPECT_FALSE(watchdog.snapshotThreads().empty());
	watchdog.checkNow();
	watchdog.removeProbe(probe);
	watchdog.removeDumpListener(listener);
}
