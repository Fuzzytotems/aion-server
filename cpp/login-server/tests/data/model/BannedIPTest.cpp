#include <chrono>
#include <unordered_set>

#include <gtest/gtest.h>

#include "DataTestUtils.h"
#include "aion/loginserver/model/BannedIP.h"
#include "aion/loginserver/model/base/BannedMacEntry.h"

using namespace aion::loginserver::model;
using namespace aion::loginserver::test;
using aion::commons::database::Timestamp;
using namespace std::chrono_literals;

TEST(BannedIPTest, DefaultsAndAccessors) {
	BannedIP ban;
	EXPECT_FALSE(ban.getId());
	EXPECT_EQ(ban.getMask(), "");
	EXPECT_FALSE(ban.getTimeEnd());
	ban.setId(3);
	ban.setMask("1.2.3.4");
	ban.setTimeEnd(Timestamp(99ms));
	EXPECT_EQ(ban.getId(), 3);
	EXPECT_EQ(ban.getMask(), "1.2.3.4");
	EXPECT_EQ(ban.getTimeEnd(), Timestamp(99ms));
}

TEST(BannedIPTest, IsActive) {
	BannedIP ban;
	ban.setMask("1.2.3.4");
	EXPECT_TRUE(ban.isActive()); // no end
	ban.setTimeEnd(nowSeconds() + 1h);
	EXPECT_TRUE(ban.isActive());
	ban.setTimeEnd(nowSeconds() - 1s);
	EXPECT_FALSE(ban.isActive());
}

TEST(BannedIPTest, EqualityAndHashAreBasedOnMask) {
	BannedIP a;
	a.setMask("1.2.3.4");
	a.setId(1);
	BannedIP b;
	b.setMask("1.2.3.4");
	b.setTimeEnd(Timestamp(5ms));
	BannedIP c;
	c.setMask("1.2.3.*");
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
	EXPECT_EQ(static_cast<uint32_t>(a.hashCode()), 0x71668980u); // Java "1.2.3.4".hashCode()
	EXPECT_EQ(BannedIP().hashCode(), 0);

	std::unordered_set<BannedIP> set;
	EXPECT_TRUE(set.insert(a).second);
	EXPECT_FALSE(set.insert(b).second); // Java: HashSet.add returns false
	EXPECT_TRUE(set.insert(c).second);
	EXPECT_EQ(set.size(), 2u);
	EXPECT_EQ(set.find(b)->getId(), 1);
}

TEST(BannedMacEntryTest, ConstructFromMillis) {
	int64_t end = millis(nowSeconds() + 10min);
	base::BannedMacEntry entry("aa-bb-cc-dd-ee-ff", end);
	EXPECT_EQ(entry.getMac(), "aa-bb-cc-dd-ee-ff");
	EXPECT_EQ(entry.getDetails(), "");
	ASSERT_TRUE(entry.getTime());
	EXPECT_EQ(millis(*entry.getTime()), end);
	EXPECT_TRUE(entry.isActive());

	entry.updateTime(millis(nowSeconds() - 1s));
	EXPECT_FALSE(entry.isActive());
	entry.setDetails("banned by GM");
	EXPECT_EQ(entry.getDetails(), "banned by GM");
}

TEST(BannedMacEntryTest, ConstructWithTimestampAndDetails) {
	base::BannedMacEntry entry("11-22-33-44-55-66", nowSeconds() + 1h, "details");
	EXPECT_EQ(entry.getMac(), "11-22-33-44-55-66");
	EXPECT_EQ(entry.getDetails(), "details");
	EXPECT_TRUE(entry.isActive());

	base::BannedMacEntry noTime("11-22-33-44-55-66", std::nullopt, "");
	EXPECT_FALSE(noTime.getTime());
	EXPECT_FALSE(noTime.isActive()); // unlike BannedIP, a ban without end time is not active
}
