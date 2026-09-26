// Lock-order validation of the collection Monitors (design §4.2, RR-1, RR-9): stripe and collection Monitors are tracked with their per-field
// lock classes, and every test of this executable fails if the validator reports a lock-order cycle (conventions: "Lock-order validator
// reports fail tests").

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

/** Fails the running test when the validator reported a new lock-order cycle during it. */
class CollectionsLockdepListener : public ::testing::EmptyTestEventListener {
public:
	void OnTestStart(const ::testing::TestInfo&) override { failuresAtStart = LockOrderValidator::getInstance().failureCount(); }
	void OnTestEnd(const ::testing::TestInfo& info) override {
		uint64_t failures = LockOrderValidator::getInstance().failureCount();
		if (failures > failuresAtStart) {
			std::string text;
			for (const LockOrderValidator::Report& report : LockOrderValidator::getInstance().getReports())
				if (report.kind == LockOrderValidator::ReportKind::CYCLE)
					text += report.text + "\n";
			ADD_FAILURE() << "lock-order cycle(s) during " << info.name() << ":\n" << text;
		}
	}

private:
	uint64_t failuresAtStart = 0;
};

[[maybe_unused]] const bool listenerInstalled = [] {
	LockOrderValidator::getInstance().setFailOnReport(true);
	::testing::UnitTest::GetInstance()->listeners().Append(new CollectionsLockdepListener());
	return true;
}();

class CollectionsLockdepTest : public CollectionsTest {};

bool hasCycleReport(const std::string& lockClass) {
	for (const LockOrderValidator::Report& report : LockOrderValidator::getInstance().getReports()) {
		if (report.kind == LockOrderValidator::ReportKind::CYCLE && (report.text.find(lockClass) != std::string::npos))
			return true;
	}
	return false;
}

} // namespace

TEST_F(CollectionsLockdepTest, StripeMonitorsAreTrackedWithTheirLockClass) {
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	if (!validator.isEnabled())
		GTEST_SKIP() << "lock-order validation is active in checked builds only";
	ConcurrentHashMap<int32_t, int32_t> map{AION_LOCK_CLASS(CollectionsLockdepTest::inversionMap#stripe)};
	Monitor teamLock{AION_LOCK_CLASS(CollectionsLockdepTest::teamLock)};
	map.compute(1, [&](std::optional<int32_t>) {
		SYNCHRONIZED(teamLock) { // stripe -> teamLock
			return std::optional<int32_t>(1);
		}
	});
	EXPECT_FALSE(hasCycleReport("CollectionsLockdepTest::inversionMap#stripe"));
	SYNCHRONIZED(teamLock) {
		map.put(2, 2); // teamLock -> stripe: closes the cycle (Java would have the same potential deadlock)
	}
	EXPECT_TRUE(hasCycleReport("CollectionsLockdepTest::inversionMap#stripe"));
	validator.clearReports(true); // provoked on purpose: does not fail the test
}

TEST_F(CollectionsLockdepTest, CollectionMonitorsAreTrackedWithTheirLockClass) {
	LockOrderValidator& validator = LockOrderValidator::getInstance();
	if (!validator.isEnabled())
		GTEST_SKIP() << "lock-order validation is active in checked builds only";
	ArrayList<int32_t> list{AION_LOCK_CLASS(CollectionsLockdepTest::inversionList)};
	HashMap<int32_t, int32_t> map{AION_LOCK_CLASS(CollectionsLockdepTest::inversionHashMap)};
	list.add(2);
	list.add(1);
	list.sort([&map](int32_t a, int32_t b) { return a - b + (map.containsKey(a) ? 0 : 0); }); // list -> map (comparator)
	EXPECT_FALSE(hasCycleReport("CollectionsLockdepTest::inversionList"));
	map.compute(1, [&list](std::optional<int32_t>) { return std::optional<int32_t>(list.size() + list.indexOf(1)); }); // map -> list
	EXPECT_TRUE(hasCycleReport("CollectionsLockdepTest::inversionList"));
	validator.clearReports(true);
}
