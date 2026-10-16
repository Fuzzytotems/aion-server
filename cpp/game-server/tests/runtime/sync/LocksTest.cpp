// RankedMutex ranks (C6/C14), StampedLock and Semaphore Java semantics (design §3.4), BlockingRegion records (§1.2).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "SyncTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/runtime/sync/Semaphore.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

// ------------------------------------------------------------------------------------------------------------------------------ RankedMutex

TEST(RankedMutexTest, RanksMustIncreaseAndSameRankNestingIsRejected) {
	RankedMutex<LockRank::CONTAINER_SLOT> slot;
	RankedMutex<LockRank::SCHEDULER> scheduler;
	RankedMutex<LockRank::SCHEDULER> otherScheduler;
	RankedMutex<LockRank::LOGGING> logging;
	ThreadContext& context = ThreadContext::current();
	{
		std::scoped_lock a(slot);
		std::scoped_lock b(scheduler);
		std::scoped_lock c(logging);
		EXPECT_EQ(context.heldLeafCount, 3u);
		EXPECT_THROW(otherScheduler.lock(), IllegalStateException);
		EXPECT_THROW((void)otherScheduler.try_lock(), IllegalStateException);
	}
	EXPECT_EQ(context.heldLeafCount, 0u);
	{
		std::scoped_lock b(scheduler);
		EXPECT_THROW(slot.lock(), IllegalStateException);
	}
	EXPECT_EQ(RankedMutex<LockRank::STATS>::RANK, LockRank::STATS);
}

TEST(RankedMutexTest, OutOfOrderUnlockAndHeldRecords) {
	RankedMutex<LockRank::IDFACTORY> idFactory;
	RankedMutex<LockRank::RECLAIMER> reclaimer;
	ThreadContext& context = ThreadContext::current();
	uint32_t before = context.heldLockCount.load();
	idFactory.lock();
	reclaimer.lock();
	EXPECT_EQ(context.heldLockCount.load(), before + 2);
	EXPECT_STREQ(context.heldLocks[before].lockClassName.load(), "IDFACTORY");
	EXPECT_EQ(context.heldLocks[before + 1].rank.load(), static_cast<uint8_t>(LockRank::RECLAIMER));
	idFactory.unlock();
	EXPECT_EQ(context.heldLeafCount, 1u);
	EXPECT_EQ(context.heldLeafRanks[0], static_cast<uint8_t>(LockRank::RECLAIMER));
	reclaimer.unlock();
	EXPECT_EQ(context.heldLeafCount, 0u);
	EXPECT_EQ(context.heldLockCount.load(), before);
}

TEST(RankedMutexTest, ContentionAndWaitRecords) {
	HangGuard guard(30s);
	RankedMutex<LockRank::SCHEDULER> mutex;
	int64_t counter = 0;
	std::vector<std::thread> threads;
	for (int t = 0; t < 6; ++t)
		threads.emplace_back([&] {
			for (int i = 0; i < 20000; ++i) {
				std::scoped_lock lock(mutex);
				++counter;
			}
		});
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_EQ(counter, 6 * 20000);

	std::promise<void> release;
	std::promise<void> locked;
	std::thread holder([&] {
		std::scoped_lock lock(mutex);
		locked.set_value();
		release.get_future().wait();
	});
	locked.get_future().wait();
	std::atomic<uint64_t> waiterId{0};
	std::thread waiter([&] {
		waiterId = ThreadContext::current().threadId();
		std::scoped_lock lock(mutex);
	});
	EXPECT_TRUE(waitUntil([&] { return waiterId.load() != 0 && threadWaitsFor(waiterId.load(), reinterpret_cast<uintptr_t>(&mutex)); }));
	release.set_value();
	holder.join();
	waiter.join();
}

// ------------------------------------------------------------------------------------------------------------------------------ StampedLock

TEST(StampedLockTest, JavaStampSemantics) {
	StampedLock lock;
	int64_t optimistic = lock.tryOptimisticRead();
	EXPECT_NE(optimistic, 0);
	EXPECT_TRUE(lock.validate(optimistic));
	EXPECT_FALSE(lock.validate(0));

	int64_t read1 = lock.readLock();
	int64_t read2 = lock.tryReadLock();
	EXPECT_NE(read1, 0);
	EXPECT_NE(read2, 0);
	EXPECT_EQ(lock.getReadLockCount(), 2);
	EXPECT_TRUE(lock.validate(optimistic)) << "read locks do not invalidate optimistic stamps";
	EXPECT_EQ(lock.tryWriteLock(), 0);
	lock.unlockRead(read1);
	lock.unlock(read2);
	EXPECT_FALSE(lock.isReadLocked());

	int64_t write = lock.writeLock();
	EXPECT_TRUE(lock.isWriteLocked());
	EXPECT_EQ(lock.tryOptimisticRead(), 0);
	EXPECT_EQ(lock.tryReadLock(), 0);
	EXPECT_TRUE(lock.validate(write));
	EXPECT_FALSE(lock.validate(optimistic));
	EXPECT_THROW(lock.unlockRead(write), IllegalMonitorStateException);
	EXPECT_THROW(lock.unlockWrite(write + (int64_t{1} << 32)), IllegalMonitorStateException);
	lock.unlockWrite(write);
	EXPECT_THROW(lock.unlockWrite(write), IllegalMonitorStateException);
	EXPECT_FALSE(lock.validate(write));
	EXPECT_THROW(lock.unlockRead(read1), IllegalMonitorStateException);
	EXPECT_EQ(ThreadContext::current().heldLockCount.load(), 0u);
}

TEST(StampedLockTest, WritersExcludeReadersAcrossThreads) {
	HangGuard guard(60s);
	StampedLock lock{AION_LOCK_CLASS(StampedLockTest::lock)};
	std::atomic<int> readers{0};
	std::atomic<int> writers{0};
	std::atomic<bool> violation{false};
	int64_t shared = 0;
	std::vector<std::thread> threads;
	for (int t = 0; t < 6; ++t)
		threads.emplace_back([&, t] {
			for (int i = 0; i < 5000; ++i) {
				if ((i + t) % 5 == 0) {
					int64_t stamp = lock.writeLock();
					if (writers.fetch_add(1) != 0 || readers.load() != 0)
						violation = true;
					++shared;
					writers.fetch_sub(1);
					lock.unlockWrite(stamp);
				} else {
					int64_t stamp = lock.readLock();
					readers.fetch_add(1);
					if (writers.load() != 0)
						violation = true;
					readers.fetch_sub(1);
					lock.unlockRead(stamp);
				}
			}
		});
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_FALSE(violation.load());
	EXPECT_EQ(shared, 6 * 1000);
}

// ------------------------------------------------------------------------------------------------------------------------------ Semaphore

TEST(SemaphoreTest, JavaPermitSemantics) {
	Semaphore semaphore(2);
	EXPECT_FALSE(semaphore.isFair());
	EXPECT_TRUE(semaphore.tryAcquire(2));
	EXPECT_FALSE(semaphore.tryAcquire());
	EXPECT_EQ(semaphore.availablePermits(), 0);
	semaphore.release(3); // may exceed the initial count
	EXPECT_EQ(semaphore.availablePermits(), 3);
	EXPECT_EQ(semaphore.drainPermits(), 3);
	EXPECT_EQ(semaphore.drainPermits(), 0);
	EXPECT_THROW(semaphore.acquire(-1), IllegalArgumentException);
	EXPECT_THROW(semaphore.release(-1), IllegalArgumentException);

	Semaphore negative(-2, true);
	EXPECT_TRUE(negative.isFair());
	EXPECT_EQ(negative.availablePermits(), -2);
	EXPECT_FALSE(negative.tryAcquire());
	negative.release(3);
	EXPECT_TRUE(negative.tryAcquire());
	EXPECT_EQ(negative.drainPermits(), 0);
	Semaphore drained(-5);
	EXPECT_EQ(drained.drainPermits(), -5);
	EXPECT_EQ(drained.availablePermits(), 0);
	EXPECT_TRUE(drained.tryAcquire(0));
}

TEST(SemaphoreTest, TimedAcquireTimesOutAndBlockedAcquireIsWokenInsideABlockingRegion) {
	HangGuard guard(30s);
	Semaphore semaphore{AION_LOCK_CLASS(AbstractCronTask::semaphore), 1};
	semaphore.acquireUninterruptibly();
	auto start = std::chrono::steady_clock::now();
	EXPECT_FALSE(semaphore.tryAcquire(30ms));
	EXPECT_GE(std::chrono::steady_clock::now() - start, 25ms);

	std::atomic<uint64_t> waiterId{0};
	std::atomic<bool> acquired{false};
	std::thread waiter([&] {
		waiterId = ThreadContext::current().threadId();
		semaphore.acquire();
		acquired = true;
		semaphore.release();
	});
	ASSERT_TRUE(waitUntil([&] {
		bool blocked = false;
		if (waiterId.load() != 0)
			withThread(waiterId.load(), [&](const ThreadContext& context) {
				ThreadContext::BlockingSnapshot blocking = context.blocking();
				ThreadContext::WaitSnapshot wait = context.wait();
				blocked = blocking.active && std::string(blocking.what) == "Semaphore.acquire" && wait.waiting &&
					std::string(wait.lockClassName) == "AbstractCronTask::semaphore";
			});
		return blocked;
	}));
	EXPECT_FALSE(acquired.load());
	semaphore.release();
	waiter.join();
	EXPECT_TRUE(acquired.load());
	EXPECT_EQ(semaphore.availablePermits(), 1);
	EXPECT_TRUE(semaphore.tryAcquire(1, 1s));
	semaphore.release();
}

TEST(SemaphoreTest, BoundsConcurrencyForFairAndNonFairSemaphores) {
	HangGuard guard(60s);
	for (bool fair : {false, true}) {
		Semaphore semaphore(3, fair);
		std::atomic<int> inside{0};
		std::atomic<int> maxInside{0};
		std::vector<std::thread> threads;
		for (int t = 0; t < 8; ++t)
			threads.emplace_back([&, t] {
				for (int i = 0; i < 2000; ++i) {
					int permits = 1 + (i + t) % 2;
					if (i % 3 == 0) {
						if (!semaphore.tryAcquire(permits, std::chrono::microseconds(100)))
							continue;
					} else {
						semaphore.acquire(permits);
					}
					int now = inside.fetch_add(permits) + permits;
					int seen = maxInside.load();
					while (now > seen && !maxInside.compare_exchange_weak(seen, now)) {
					}
					inside.fetch_sub(permits);
					semaphore.release(permits);
				}
			});
		for (std::thread& thread : threads)
			thread.join();
		EXPECT_LE(maxInside.load(), 3) << "fair=" << fair;
		EXPECT_EQ(semaphore.availablePermits(), 3);
	}
}

// ------------------------------------------------------------------------------------------------------------------------------ BlockingRegion

TEST(BlockingRegionTest, OutermostRegionIsRecorded) {
	ThreadContext& context = ThreadContext::current();
	EXPECT_FALSE(context.blocking().active);
	{
		BlockingRegion outer("DAO.execute");
		{
			BlockingRegion inner("Future.get");
			ThreadContext::BlockingSnapshot snapshot = context.blocking();
			EXPECT_TRUE(snapshot.active);
			EXPECT_STREQ(snapshot.what, "DAO.execute");
			EXPECT_NE(std::string(snapshot.where.file_name()).find("LocksTest"), std::string::npos);
		}
		EXPECT_TRUE(context.blocking().active);
	}
	EXPECT_FALSE(context.blocking().active);
	EXPECT_EQ(context.blockingDepth, 0u);
}
