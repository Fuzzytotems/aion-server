// Lock-order validator (design §4.2, C6, D5): inversions across threads with both stacks, dedup, same-class nesting, suppression, blocking
// under a Monitor, failOnReport. Checked builds only (the validator records nothing in release builds).

#include <gtest/gtest.h>

#include <chrono>

#include <string>
#include <thread>

#include "LockdepTestSupport.h"
#include "SyncTestSupport.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

AION_LOCKDEP_FAIL_TESTS_ON_CYCLES();

namespace {

using Kind = LockOrderValidator::ReportKind;

/** Not constexpr, so tests after the skip are not unreachable code in release builds (C4702). */
bool validatorActive() {
	return LockOrderValidator::getInstance().isEnabled();
}

class LockOrderValidatorTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!validatorActive())
			GTEST_SKIP() << "the lock-order validator is active in checked builds only";
		validator().clearReports(true);
	}
	void TearDown() override { validator().clearReports(true); }

	static LockOrderValidator& validator() { return LockOrderValidator::getInstance(); }

	static std::vector<LockOrderValidator::Report> reportsOf(Kind kind) {
		std::vector<LockOrderValidator::Report> reports;
		for (auto& report : validator().getReports())
			if (report.kind == kind)
				reports.push_back(report);
		return reports;
	}
};

void lockInOrder(Monitor& outer, Monitor& inner) {
	SYNCHRONIZED(outer) {
		SYNCHRONIZED(inner) {
		}
	}
}

} // namespace

TEST_F(LockOrderValidatorTest, InversionAcrossTwoThreadsIsReportedWithBothStacksWithoutDeadlock) {
	HangGuard guard(60s);
	Monitor a{AION_LOCK_CLASS(LockdepTest::inversionA)};
	Monitor b{AION_LOCK_CLASS(LockdepTest::inversionB)};
	// sequential threads: the order is inverted, but the threads never run the critical sections at the same time, so nothing deadlocks
	std::thread([&] { lockInOrder(a, b); }).join();
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 0u);
	std::thread([&] { lockInOrder(b, a); }).join();

	std::vector<LockOrderValidator::Report> cycles = reportsOf(Kind::CYCLE);
	ASSERT_EQ(cycles.size(), 1u);
	const LockOrderValidator::Report& report = cycles[0];
	EXPECT_EQ(report.heldLockClass, "LockdepTest::inversionB");
	EXPECT_EQ(report.acquiredLockClass, "LockdepTest::inversionA");
	EXPECT_FALSE(report.acquisitionStack.empty());
	EXPECT_FALSE(report.reverseEdgeStack.empty());
	EXPECT_NE(report.text.find("LockdepTest::inversionA"), std::string::npos);
	EXPECT_NE(report.text.find("LockdepTest::inversionB"), std::string::npos);
	EXPECT_NE(report.acquisitionStack.find("lockInOrder"), std::string::npos) << report.acquisitionStack;
	EXPECT_NE(report.reverseEdgeStack.find("lockInOrder"), std::string::npos) << report.reverseEdgeStack;
	EXPECT_EQ(validator().failureCount(), 1u) << "failOnReport is set by LockdepTestSupport";

	// the same inversion again is not reported again
	std::thread([&] { lockInOrder(b, a); }).join();
	std::thread([&] { lockInOrder(a, b); }).join();
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 1u);
	validator().clearReports(); // intentional inversion: do not fail the test through the listener
}

TEST_F(LockOrderValidatorTest, LongerCyclesAndConsistentOrders) {
	Monitor a{AION_LOCK_CLASS(LockdepTest::cycleA)};
	Monitor b{AION_LOCK_CLASS(LockdepTest::cycleB)};
	Monitor c{AION_LOCK_CLASS(LockdepTest::cycleC)};
	lockInOrder(a, b);
	lockInOrder(b, c);
	lockInOrder(a, c);
	SYNCHRONIZED(a) {
		lockInOrder(b, c);
	}
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 0u);
	lockInOrder(c, a); // closes a -> b -> c -> a (and a -> c -> a)
	std::vector<LockOrderValidator::Report> cycles = reportsOf(Kind::CYCLE);
	ASSERT_EQ(cycles.size(), 1u);
	EXPECT_EQ(cycles[0].heldLockClass, "LockdepTest::cycleC");
	EXPECT_EQ(cycles[0].acquiredLockClass, "LockdepTest::cycleA");
	validator().clearReports();
}

TEST_F(LockOrderValidatorTest, ReentrancyIsNotNestingButSameClassNestingWarns) {
	Monitor first{AION_LOCK_CLASS(LockdepTest::player)};
	Monitor second{AION_LOCK_CLASS(LockdepTest::player)};
	SYNCHRONIZED(first) {
		SYNCHRONIZED(first) {
		}
	}
	EXPECT_TRUE(validator().getReports().empty());
	lockInOrder(first, second);
	lockInOrder(second, first);
	std::vector<LockOrderValidator::Report> nesting = reportsOf(Kind::SAME_CLASS_NESTING);
	ASSERT_EQ(nesting.size(), 1u);
	EXPECT_EQ(nesting[0].heldLockClass, "LockdepTest::player");
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 0u);
	EXPECT_EQ(validator().failureCount(), 0u);
}

TEST_F(LockOrderValidatorTest, SuppressedSitesRecordNoEdges) {
	Monitor a{AION_LOCK_CLASS(LockdepTest::suppressedA)};
	Monitor b{AION_LOCK_CLASS(LockdepTest::suppressedB)};
	{
		LockdepSuppression suppression("test: order is guaranteed by a higher-level protocol");
		lockInOrder(a, b);
	}
	lockInOrder(b, a);
	EXPECT_TRUE(validator().getReports().empty());
	{
		LockdepSuppression suppression("test");
		lockInOrder(a, b); // would close the cycle
	}
	EXPECT_TRUE(validator().getReports().empty());
}

TEST_F(LockOrderValidatorTest, TryLockRecordsNoEdgeButCountsAsHeld) {
	Monitor a{AION_LOCK_CLASS(LockdepTest::tryA)};
	Monitor b{AION_LOCK_CLASS(LockdepTest::tryB)};
	SYNCHRONIZED(b) {
		ASSERT_TRUE(a.tryLock()); // b -> a not recorded: tryLock cannot block
		a.unlock();
	}
	ASSERT_TRUE(a.tryLock());
	SYNCHRONIZED(b) { // a -> b recorded from the try-locked a
	}
	a.unlock();
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 0u);
	lockInOrder(b, a); // b -> a closes the cycle
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 1u);
	validator().clearReports();
}

// Review finding: a timed tryLock can block, so its edge is recorded even when the first attempt succeeds (uncontended in tests).
TEST_F(LockOrderValidatorTest, UncontendedTimedTryLockRecordsItsEdge) {
	Monitor a{AION_LOCK_CLASS(LockdepTest::timedA)};
	Monitor b{AION_LOCK_CLASS(LockdepTest::timedB)};
	SYNCHRONIZED(a) {
		ASSERT_TRUE(b.tryLock(std::chrono::seconds(1))); // free: acquired at once, a -> b must still be recorded
		b.unlock();
	}
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 0u);
	lockInOrder(b, a); // b -> a closes the cycle
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 1u);
	validator().clearReports();
}

TEST_F(LockOrderValidatorTest, TryLockOverloadsReportTheDynamicClass) {
	Monitor anonymousOuter;
	Monitor anonymousInner;
	const LockClass& outerClass = LockClass::named("LockdepTest::DynamicOuter");
	const LockClass& innerClass = LockClass::named("LockdepTest::DynamicInner");
	ASSERT_TRUE(anonymousOuter.tryLock(outerClass));
	ASSERT_TRUE(anonymousInner.tryLock(std::chrono::seconds(1), innerClass));
	anonymousInner.unlock();
	anonymousOuter.unlock();
	EXPECT_TRUE(validator().getReports().empty()) << "distinct dynamic classes are not same-class nesting";
	anonymousInner.lock(innerClass);
	anonymousOuter.lock(outerClass); // inner -> outer closes the cycle with outer -> inner from the timed tryLock
	anonymousOuter.unlock();
	anonymousInner.unlock();
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 1u);
	validator().clearReports();
}

TEST_F(LockOrderValidatorTest, BlockingUnderMonitorIsCountedPerSite) {
	Monitor monitor{AION_LOCK_CLASS(LockdepTest::blocking)};
	{
		BlockingRegion outside("Test.outside");
	}
	EXPECT_TRUE(validator().getReports().empty());
	for (int i = 0; i < 3; ++i) {
		SYNCHRONIZED(monitor) {
			BlockingRegion region("Test.blockingCall");
		}
	}
	std::vector<LockOrderValidator::Report> blocking = reportsOf(Kind::BLOCKING_UNDER_MONITOR);
	ASSERT_EQ(blocking.size(), 1u);
	EXPECT_EQ(blocking[0].occurrences, 3u);
	EXPECT_EQ(blocking[0].heldLockClass, "LockdepTest::blocking");
	EXPECT_NE(blocking[0].text.find("Test.blockingCall"), std::string::npos);
	bool listed = false;
	for (const std::string& line : validator().describe())
		listed = listed || line.find("Test.blockingCall") != std::string::npos;
	EXPECT_TRUE(listed);
}

TEST_F(LockOrderValidatorTest, NonReentrantSelfAcquisitionIsReported) {
	StampedLock lock{AION_LOCK_CLASS(LockdepTest::stamped)};
	int64_t first = lock.readLock();
	int64_t second = lock.readLock(); // shared + shared: legal
	EXPECT_TRUE(validator().getReports().empty());
	lock.unlockRead(second);
	// a writeLock now would block forever (Java semantics); drive the hook directly as StampedLock::writeLock does before blocking
	validator().beforeAcquire(LockClass::named("LockdepTest::stamped"), reinterpret_cast<uintptr_t>(&lock), false);
	std::vector<LockOrderValidator::Report> nesting = reportsOf(Kind::SAME_CLASS_NESTING);
	ASSERT_EQ(nesting.size(), 1u);
	EXPECT_NE(nesting[0].text.find("self-deadlock"), std::string::npos);
	lock.unlockRead(first);
}

TEST_F(LockOrderValidatorTest, DescribeAndDisable) {
	Monitor a{AION_LOCK_CLASS(LockdepTest::describeA)};
	Monitor b{AION_LOCK_CLASS(LockdepTest::describeB)};
	lockInOrder(a, b);
	std::vector<std::string> lines = validator().describe();
	ASSERT_FALSE(lines.empty());
	EXPECT_NE(lines[0].find("1 edges"), std::string::npos) << lines[0];

	validator().setEnabled(false);
	lockInOrder(b, a);
	validator().setEnabled(true);
	EXPECT_EQ(validator().reportCount(Kind::CYCLE), 0u);
}
