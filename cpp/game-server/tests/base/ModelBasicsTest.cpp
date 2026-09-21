// P4-05 direct model classes and utils with hand-derived expectations: GameTime, Announcement, Expirable.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/Announcement.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/time/gametime/DayTime.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver {
namespace {

using utils::time::gametime::DayTime;
using utils::time::gametime::GameTime;

constexpr int32_t MINUTES_IN_DAY = 24 * 60;
constexpr int32_t MINUTES_IN_MONTH = 31 * MINUTES_IN_DAY;
constexpr int32_t MINUTES_IN_YEAR = 12 * MINUTES_IN_MONTH;

TEST(GameTimeTest, CalendarFields) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<GameTime> start = GameTime::create(std::nullopt);
	EXPECT_EQ(start->getTime(), 0);
	EXPECT_EQ(start->getYear(), 0);
	EXPECT_EQ(start->getMonth(), 1);
	EXPECT_EQ(start->getDay(), 1);
	EXPECT_EQ(start->getHour(), 0);
	EXPECT_EQ(start->getMinute(), 0);
	EXPECT_EQ(start->getDayTime(), DayTime::NIGHT);

	runtime::Ref<GameTime> lastMinuteOfJanuary = GameTime::create(MINUTES_IN_MONTH - 1);
	EXPECT_EQ(lastMinuteOfJanuary->getMonth(), 1);
	EXPECT_EQ(lastMinuteOfJanuary->getDay(), 31);
	EXPECT_EQ(lastMinuteOfJanuary->getHour(), 23);
	EXPECT_EQ(lastMinuteOfJanuary->getMinute(), 59);

	runtime::Ref<GameTime> february = GameTime::create(MINUTES_IN_MONTH);
	EXPECT_EQ(february->getMonth(), 2);
	EXPECT_EQ(february->getDay(), 1); // equal minutes: day 1 of the following month

	runtime::Ref<GameTime> nextYear = GameTime::create(MINUTES_IN_YEAR + 2 * MINUTES_IN_DAY + 9 * 60 + 5);
	EXPECT_EQ(nextYear->getYear(), 1);
	EXPECT_EQ(nextYear->getMonth(), 1);
	EXPECT_EQ(nextYear->getDay(), 3);
	EXPECT_EQ(nextYear->getHour(), 9);
	EXPECT_EQ(nextYear->getMinute(), 5);
	EXPECT_EQ(nextYear->getDayTime(), DayTime::AFTERNOON);
}

TEST(GameTimeTest, DayTimeBoundaries) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_EQ(GameTime::create(3 * 60)->getDayTime(), DayTime::NIGHT);
	EXPECT_EQ(GameTime::create(4 * 60)->getDayTime(), DayTime::MORNING);
	EXPECT_EQ(GameTime::create(8 * 60 + 59)->getDayTime(), DayTime::MORNING);
	EXPECT_EQ(GameTime::create(9 * 60)->getDayTime(), DayTime::AFTERNOON);
	EXPECT_EQ(GameTime::create(16 * 60)->getDayTime(), DayTime::AFTERNOON);
	EXPECT_EQ(GameTime::create(17 * 60)->getDayTime(), DayTime::EVENING);
	EXPECT_EQ(GameTime::create(21 * 60 + 59)->getDayTime(), DayTime::EVENING);
	EXPECT_EQ(GameTime::create(22 * 60)->getDayTime(), DayTime::NIGHT);
}

TEST(GameTimeTest, ArithmeticAndIdentity) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_THROW(GameTime::create(-1), runtime::IllegalArgumentException);
	runtime::Ref<GameTime> a = GameTime::create(100);
	runtime::Ref<GameTime> b = GameTime::create(40);
	EXPECT_EQ(a->minus(*b)->getTime(), 60);
	EXPECT_EQ(a->plus(*b)->getTime(), 140);
	EXPECT_THROW(b->minus(*a), runtime::IllegalArgumentException); // Java: new GameTime(-60)
	EXPECT_TRUE(a->isGreaterThan(*b));
	EXPECT_TRUE(b->isLessThan(*a));
	EXPECT_EQ(a->hashCode(), 131);
	runtime::Ref<GameTime> copy = a->clone();
	EXPECT_NE(copy, a);
	EXPECT_TRUE(copy->equals(*a));
	EXPECT_FALSE(copy->equals(*b));
	EXPECT_FALSE(a->setDayTime(DayTime::NIGHT)); // 01:40 is night already
	EXPECT_TRUE(a->setDayTime(DayTime::EVENING));
	EXPECT_EQ(a->getDayTime(), DayTime::EVENING);
	a->addMinutes(0);
	EXPECT_EQ(a->getTime(), 100);
	a->addMinutes(5); // 01:45, no hour change
	EXPECT_EQ(a->getTime(), 105);
	// reaching a full hour calls onHourChange: TemporarySpawnEngine.onHourChange (nothing registered here) and the day time of 02:00; the weather is
	// only checked when the clock advanced by one minute (15 minutes stand for an admin change)
	EXPECT_NO_THROW(a->addMinutes(15));
	EXPECT_EQ(a->getTime(), 120);
	EXPECT_EQ(a->getDayTime(), DayTime::NIGHT) << "02:00 is night: setDayTime(calculateDayTime()) replaced EVENING";
	a->addMinutes(60 * 3); // 05:00, not by the clock
	EXPECT_EQ(a->getDayTime(), DayTime::MORNING);
}

TEST(AnnouncementTest, FactionAndChatType) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<model::Announcement> announcement = model::Announcement::create(3, "Hello", "elyos", "wHiTe", 60);
	EXPECT_EQ(announcement->getId(), 3);
	EXPECT_EQ(announcement->getAnnounce(), "Hello");
	EXPECT_EQ(announcement->getFaction(), model::Race::ELYOS);
	EXPECT_EQ(announcement->getType(), "wHiTe");
	EXPECT_EQ(announcement->getChatType(), model::ChatType::WHITE_CENTER);
	EXPECT_EQ(announcement->getDelay(), 60);
	EXPECT_EQ(model::Announcement::create(1, "", "ASMODIANS", "System", 0)->getFaction(), model::Race::ASMODIANS);
	EXPECT_EQ(model::Announcement::create(1, "", "ALL", "System", 0)->getFaction(), std::nullopt);
	EXPECT_EQ(model::Announcement::create(1, "", "ALL", "System", 0)->getChatType(), model::ChatType::GOLDEN_YELLOW);
	EXPECT_EQ(model::Announcement::create(1, "", "ALL", "yellow", 0)->getChatType(), model::ChatType::YELLOW_CENTER);
	EXPECT_EQ(model::Announcement::create(1, "", "ALL", "SHOUT", 0)->getChatType(), model::ChatType::SHOUT);
	EXPECT_EQ(model::Announcement::create(1, "", "ALL", "Orange", 0)->getChatType(), model::ChatType::GROUP_LEADER);
	EXPECT_EQ(model::Announcement::create(1, "", "ALL", "Blue", 0)->getChatType(), model::ChatType::BRIGHT_YELLOW_CENTER);
}

class TestExpirable final : public model::Expirable {
public:
	explicit TestExpirable(int32_t expireTime) : expireTime(expireTime) {}
	int32_t getExpireTime() override { return expireTime; }
	void onExpire(model::gameobjects::player::Player&) override {}
	void retain() const noexcept override {}
	void release() const noexcept override {}

private:
	int32_t expireTime;
};

TEST(ExpirableTest, DefaultMethods) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	TestExpirable never(0);
	EXPECT_EQ(never.secondsUntilExpiration(), 0);
	EXPECT_FALSE(never.isExpired());
	EXPECT_TRUE(never.canExpireNow());

	int32_t now = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	TestExpirable past(now - 100);
	EXPECT_LE(past.secondsUntilExpiration(), -100);
	EXPECT_TRUE(past.isExpired());
	TestExpirable future(now + 1000);
	EXPECT_GE(future.secondsUntilExpiration(), 998);
	EXPECT_FALSE(future.isExpired());
}

} // namespace
} // namespace aion::gameserver
