// IDFactory (design §6, RR-10/RR-16): monotone cursor, wrap, 300 s release quarantine (also across a wrap), invalid ids, DAO seeding API,
// Java warnings, checked-build reuse tracking, concurrency.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "ServicesTestSupport.h"
#include "aion/gameserver/runtime/base/Checked.h"

namespace aion::gameserver::runtime::servicestest {
namespace {

using namespace std::chrono;
using utils::idfactory::IDFactoryError;

class IDFactoryTest : public DeterministicServicesTest {
protected:
	IDFactory& factory = IDFactory::getInstance();

	void configure(int32_t wrapAt, seconds releaseDelay) {
		IDFactory::Config config;
		config.wrapAt = wrapAt;
		config.releaseDelay = releaseDelay;
		factory.configure(config);
	}
};

TEST_F(IDFactoryTest, StartsAtOneWithIdZeroLocked) {
	EXPECT_EQ(factory.getUsedCount(), 1);
	EXPECT_EQ(factory.nextId(), 1);
	EXPECT_EQ(factory.nextId(), 2);
	EXPECT_EQ(factory.getUsedCount(), 3);
	EXPECT_EQ(factory.getCursor(), 3);
	EXPECT_THROW(factory.lockIds({0}), IDFactoryError);
}

TEST_F(IDFactoryTest, ReleasedIdsAreNotReusedBeforeTheCursorWraps) {
	configure(1 << 27, seconds(0)); // no quarantine: only the cursor protects the id
	std::vector<int32_t> ids;
	for (int i = 0; i < 10; ++i)
		ids.push_back(factory.nextId());
	EXPECT_EQ(ids.front(), 1);
	factory.releaseId(3, "Npc");
	factory.releaseId(1, "Npc");
	EXPECT_EQ(factory.getUsedCount(), 9); // 0 and 8 of the 10
	EXPECT_EQ(factory.nextId(), 11);      // Java would hand out 1 again
	EXPECT_EQ(factory.getCursor(), 12);
}

TEST_F(IDFactoryTest, CursorWrapsToTheLowestFreeId) {
	configure(10, seconds(0));
	for (int32_t expected = 1; expected < 10; ++expected)
		EXPECT_EQ(factory.nextId(), expected);
	factory.releaseId(7);
	factory.releaseId(4);
	LogCapture capture("com.aionemu.gameserver.utils.idfactory.IDFactory");
	EXPECT_EQ(factory.nextId(), 4); // the cursor passed wrap_at (10): lowest free id
	EXPECT_TRUE(capture.contains("wrapped to ID 4"));
	EXPECT_EQ(factory.nextId(), 7); // cursor continues upwards from 5
	EXPECT_EQ(factory.nextId(), 10); // nothing below wrap_at is free: ids >= wrap_at are used
	EXPECT_EQ(factory.nextId(), 11);
	factory.releaseId(2);
	EXPECT_EQ(factory.nextId(), 2);
}

TEST_F(IDFactoryTest, ReleasedIdsStayQuarantinedForTheReleaseDelay) {
	configure(1 << 27, seconds(300));
	EXPECT_EQ(factory.nextId(), 1);
	EXPECT_EQ(factory.nextId(), 2);
	factory.releaseId(1, "Npc");
	EXPECT_EQ(factory.getQuarantinedCount(), 1);
	EXPECT_EQ(factory.getUsedCount(), 3); // quarantined ids count as used
	pass(seconds(299));
	EXPECT_EQ(factory.drainQuarantine(), 0);
	EXPECT_EQ(factory.getQuarantinedCount(), 1);
	pass(seconds(1));
	EXPECT_EQ(factory.drainQuarantine(), 1);
	EXPECT_EQ(factory.getQuarantinedCount(), 0);
	EXPECT_EQ(factory.getUsedCount(), 2);
	EXPECT_EQ(factory.nextId(), 3); // still no reuse: the cursor is monotone
}

TEST_F(IDFactoryTest, QuarantineHoldsAcrossAWrap) {
	configure(6, seconds(300));
	for (int32_t expected = 1; expected < 6; ++expected)
		EXPECT_EQ(factory.nextId(), expected);
	factory.releaseId(2);
	factory.releaseId(3);
	pass(seconds(100));
	EXPECT_EQ(factory.nextId(), 6); // 2 and 3 are quarantined, so the wrap finds nothing below wrap_at
	pass(seconds(199));
	EXPECT_EQ(factory.nextId(), 7);
	pass(seconds(1)); // 300 s after the release: freed lazily by the next allocation
	EXPECT_EQ(factory.nextId(), 2);
	EXPECT_EQ(factory.nextId(), 3);
	EXPECT_EQ(factory.getQuarantinedCount(), 0);
}

TEST_F(IDFactoryTest, InvalidIdsAreNeverAllocated) {
	configure(1 << 27, seconds(0));
	std::vector<int32_t> ids;
	for (int i = 0; i < 6500; ++i)
		ids.push_back(factory.nextId());
	for (int32_t invalid : {6484, 6485, 6486, 6487})
		EXPECT_EQ(std::ranges::count(ids, invalid), 0) << invalid;
	EXPECT_TRUE(std::ranges::none_of(ids, IDFactory::isInvalidId));
	EXPECT_EQ(ids.back(), 6504);
	EXPECT_TRUE(IDFactory::isInvalidId(14676));
	EXPECT_TRUE(IDFactory::isInvalidId(1812773207));
	EXPECT_FALSE(IDFactory::isInvalidId(6488));
}

TEST_F(IDFactoryTest, DaoSeedingLocksIds) {
	std::vector<int32_t> playerIds{1, 2, 5};
	std::vector<int32_t> itemIds{3, 100000};
	factory.lockIds(playerIds);
	factory.lockIds(itemIds);
	EXPECT_EQ(factory.getUsedCount(), 6);
	EXPECT_EQ(factory.nextId(), 4);
	EXPECT_EQ(factory.nextId(), 6);
	std::vector<int32_t> duplicate{7, 5};
	EXPECT_THROW(factory.lockIds(duplicate), IDFactoryError);
	EXPECT_THROW(factory.lockIds({-1}), IDFactoryError);
	LogCapture capture("com.aionemu.gameserver.utils.idfactory.IDFactory");
	factory.logUsedCount();
	EXPECT_TRUE(capture.contains("IDFactory: 9 IDs used."));
}

TEST_F(IDFactoryTest, ReleasingAnIdThatIsNotTakenWarns) {
	LogCapture capture("com.aionemu.gameserver.utils.idfactory.IDFactory");
	factory.releaseId(42);
	factory.releaseId(-5);
	int32_t id = factory.nextId();
	factory.releaseId(id);
	factory.releaseId(id); // double release while quarantined
	EXPECT_EQ(capture.count("because it wasn't taken"), 3u);
	EXPECT_TRUE(capture.contains("IllegalArgumentException")); // logged with a stack trace like Java
	EXPECT_EQ(factory.getQuarantinedCount(), 1);
	std::vector<int32_t> objects{factory.nextId(), factory.nextId()};
	factory.releaseObjectIds(objects, "Item");
	EXPECT_EQ(factory.getQuarantinedCount(), 3);
}

TEST_F(IDFactoryTest, RecentlyReleasedNamesThePreviousClass) {
	IDFactory::Config config;
	config.releaseDelay = seconds(300);
	config.reuseWarningWindow = seconds(3600);
	factory.configure(config);
	int32_t id = factory.nextId();
	factory.releaseId(id, "Npc");
	if (!AION_CHECKED) {
		EXPECT_EQ(factory.recentlyReleased(id), nullptr);
		return;
	}
	ASSERT_NE(factory.recentlyReleased(id), nullptr);
	EXPECT_STREQ(factory.recentlyReleased(id), "Npc");
	EXPECT_EQ(factory.recentlyReleased(id + 1), nullptr);
	pass(seconds(3599));
	EXPECT_STREQ(factory.recentlyReleased(id), "Npc");
	pass(seconds(2));
	EXPECT_EQ(factory.recentlyReleased(id), nullptr);
	int32_t unnamed = factory.nextId();
	factory.releaseId(unnamed);
	EXPECT_STREQ(factory.recentlyReleased(unnamed), "unknown");
}

TEST_F(IDFactoryTest, ConfigurationIsValidated) {
	IDFactory::Config config;
	config.wrapAt = 1;
	EXPECT_THROW(factory.configure(config), IllegalArgumentException);
	config.wrapAt = 100;
	config.releaseDelay = seconds(-1);
	EXPECT_THROW(factory.configure(config), IllegalArgumentException);
	config.releaseDelay = seconds(5);
	factory.configure(config);
	EXPECT_EQ(factory.getConfig().wrapAt, 100);
	EXPECT_EQ(factory.getConfig().releaseDelay, seconds(5));
}

TEST_F(IDFactoryTest, ConcurrentAllocationAndReleaseNeverHandOutALiveId) {
	configure(4096, seconds(0)); // small wrap_at and no quarantine: ids are reused as fast as possible
	constexpr int THREADS = 8;
	constexpr int ROUNDS = 4000;
	std::mutex liveMutex;
	std::set<int32_t> live;
	std::atomic<int32_t> duplicates{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < THREADS; ++t) {
		threads.emplace_back([&, t] {
			std::vector<int32_t> mine;
			for (int round = 0; round < ROUNDS; ++round) {
				int32_t id = factory.nextId();
				{
					std::scoped_lock lock(liveMutex);
					if (!live.insert(id).second)
						++duplicates;
				}
				mine.push_back(id);
				if ((round + t) % 3 != 0) {
					int32_t released = mine.back();
					mine.pop_back();
					{
						std::scoped_lock lock(liveMutex);
						live.erase(released);
					}
					factory.releaseId(released);
				}
			}
		});
	}
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_EQ(duplicates.load(), 0);
	EXPECT_EQ(factory.getUsedCount(), static_cast<int32_t>(live.size()) + 1);
}

} // namespace
} // namespace aion::gameserver::runtime::servicestest
