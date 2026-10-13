// P5-12b3, M5i I-4 (m5i-plan.md 15.3): Headhunter's kill counter and order, Event's force types and its hooks without buffs, and
// EventBuffHandler's buff data - the random pools and buff days it picks, keeps from the database and renews on a new day.
//
// Expectations are derived by hand from Headhunter.java:37-66, Event.java:70-76,187-225,279-288 and EventBuffHandler.java:45-117,206-217.
// The EventBuffHandler cases read and write the `event` table of the DAO tests' database (tests/dao/DaoTestDatabase.h) and skip without it.
// Not covered here: the buffs on players (tryBuff with SkillEngine and the effect controller), the spawns and the inventory drop of
// Event.start/stop - the gate's (I-11, the event buff).

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../dao/DaoTestDatabase.h"
#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dao/EventDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/EventData.bind.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/templates/event/Buff.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/services/event/EventBuffHandler.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::event::Headhunter;
using model::gameobjects::Persistable;
using services::event::Event;
using services::event::EventBuffHandler;

// ---------------------------------------------------------------------------------------------------------------------- Headhunter

TEST(HeadhunterTest, AKillCountsAndMarksTheHunterForAnUpdate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Headhunter> hunter = Headhunter::create(7, 4, 1000, Persistable::PersistentState::UPDATED);
	const int64_t before = commons::utils::currentTimeMillis();
	EXPECT_EQ(hunter->incrementAndGetKills(), 5);
	EXPECT_EQ(hunter->getKills(), 5);
	EXPECT_EQ(hunter->getPersistentState(), Persistable::PersistentState::UPDATE_REQUIRED);
	EXPECT_GE(hunter->getLastUpdate(), before);
	EXPECT_EQ(hunter->incrementAndGetKills(), 6);
}

TEST(HeadhunterTest, MoreKillsSortFirstThenTheLaterUpdate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<Headhunter> many = Headhunter::create(1, 10, 1000, Persistable::PersistentState::UPDATED);
	runtime::Ref<Headhunter> few = Headhunter::create(2, 3, 9000, Persistable::PersistentState::UPDATED);
	runtime::Ref<Headhunter> fewLater = Headhunter::create(3, 3, 9500, Persistable::PersistentState::UPDATED);
	runtime::Ref<Headhunter> fewSame = Headhunter::create(4, 3, 9500, Persistable::PersistentState::UPDATED);
	EXPECT_EQ(many->compareTo(*few), -1);
	EXPECT_EQ(few->compareTo(*many), 1);
	EXPECT_EQ(fewLater->compareTo(*few), -1) << "equal kills: the later update first";
	EXPECT_EQ(few->compareTo(*fewLater), 1);
	EXPECT_EQ(fewLater->compareTo(*fewSame), 1) << "Java: lastUpdate > other ? -1 : 1, so a full tie answers 1";
}

// ----------------------------------------------------------------------------------------------------------------- Event, buff data

const model::templates::event::EventTemplate& eventTemplate(const std::string& events) {
	static std::vector<std::unique_ptr<dataholders::EventData>> holders;
	static xml::LoadContext context;
	holders.push_back(xml::bindString<dataholders::EventData>(context, "<events>" + events + "</events>"));
	return holders.back()->getEvents().front();
}

TEST(EventForceTypeTest, EventForceTypesAreInternedWithThePrefix) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const skillengine::model::Effect_ForceType* first = Event::getOrCreateEffectForceType("Halloween");
	EXPECT_EQ(first, Event::getOrCreateEffectForceType("Halloween")) << "ForceType.getInstance interns the name";
	EXPECT_EQ(first->getName(), "[EVENT] Halloween");
	EXPECT_TRUE(Event::isEventEffectForceType(first));
	EXPECT_FALSE(Event::isEventEffectForceType(skillengine::model::Effect_ForceType::getInstance("Halloween")));
	EXPECT_FALSE(Event::isEventEffectForceType(nullptr));
}

class EventBuffsTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		timeZone.emplace(configs::main::GSConfig::TIME_ZONE_ID, std::chrono::locate_zone("UTC"));
		// World (EventBuffHandler.resetTodaysBuffs) needs its holders: none, published once for the executable and never reset
		static const bool worldPublished = [] {
			static xml::LoadContext worldContext;
			if (!dataholders::DataManager::WORLD_MAPS_DATA)
				dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(worldContext, "<world_maps/>"));
			if (!dataholders::DataManager::ZONE_DATA)
				dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(worldContext, "<zones/>"));
			if (!dataholders::DataManager::SHIELD_DATA)
				dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(worldContext, "<shields/>"));
			if (!dataholders::DataManager::MATERIAL_DATA)
				dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(worldContext, "<material_templates/>"));
			return true;
		}();
		static_cast<void>(worldPublished);
	}

	static std::vector<const model::templates::event::Buff*> buffsOf(const model::templates::event::EventTemplate& event) {
		std::vector<const model::templates::event::Buff*> buffs;
		for (const model::templates::event::Buff& buff : *event.getBuffs())
			buffs.push_back(&buff);
		return buffs;
	}

	static int32_t lengthOfThisMonth() {
		const std::chrono::year_month_day ymd(std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time()));
		return static_cast<int32_t>(static_cast<unsigned>(std::chrono::year_month_day_last(ymd.year(), std::chrono::month_day_last(ymd.month())).day()));
	}

	std::optional<AtomicConfigScope<const std::chrono::time_zone*>> timeZone;
};

TEST_F(EventBuffsTest, AnEventWithoutBuffsHasNoForceTypeAndTakesNoEndTasksBeforeItsStart) {
	const model::templates::event::EventTemplate& plain = eventTemplate(R"(<event name="Plain"/>)");
	runtime::Ref<Event> event = Event::create(&plain);
	EXPECT_EQ(event->getEffectForceType(), nullptr) << "no EventBuffHandler";
	EXPECT_FALSE(event->addOnEventEndTask(runtime::PinnedCallback<void()>([] {}))) << "not started";
	PlayerFixture f = makePlayer(81, 800);
	event->onEnterMap(*f.player); // no handler: nothing
	event->onTimeChanged(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()));
}

TEST_F(EventBuffsTest, ABuffHandlerPicksItsPoolAndBuffDaysAndStoresThem) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: EventBuffHandler reads and writes the event table";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	const model::templates::event::EventTemplate& event = eventTemplate(
		R"(<event name="Pools"><buffs><buff skill_ids="11 12 13 14" pool="2"><restriction random_days_per_month="3"/><trigger condition="ENTER_MAP"/></buff>)"
		R"(<buff skill_ids="21"><trigger condition="PVE_KILL"/></buff></buffs></event>)");
	runtime::Ref<EventBuffHandler> handler = EventBuffHandler::create("Pools", buffsOf(event));
	EXPECT_EQ(handler->getEffectForceType(), Event::getOrCreateEffectForceType("Pools"));

	std::optional<std::vector<dao::EventDAO::StoredBuffData>> stored = dao::EventDAO::loadStoredBuffData("Pools");
	ASSERT_TRUE(stored);
	ASSERT_EQ(stored->size(), 1u) << "only the buff with a pool and buff days is stored";
	const dao::EventDAO::StoredBuffData& data = stored->front();
	EXPECT_EQ(data.getBuffIndex(), 0);
	ASSERT_TRUE(data.getActivePoolSkillIds());
	EXPECT_EQ(data.getActivePoolSkillIds()->size(), 2u);
	for (int32_t skillId : *data.getActivePoolSkillIds())
		EXPECT_TRUE(skillId >= 11 && skillId <= 14) << skillId;
	ASSERT_TRUE(data.getAllowedBuffDays());
	EXPECT_EQ(data.getAllowedBuffDays()->size(), 3u);
	for (int32_t day : *data.getAllowedBuffDays())
		EXPECT_TRUE(day >= 1 && day <= lengthOfThisMonth()) << day;
}

TEST_F(EventBuffsTest, ValidStoredBuffDataIsKeptAndInvalidIsReplaced) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: EventBuffHandler reads and writes the event table";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	const model::templates::event::EventTemplate& event = eventTemplate(
		R"(<event name="Kept"><buffs><buff skill_ids="11 12 13 14" pool="2"><restriction random_days_per_month="3"/><trigger condition="ENTER_MAP"/></buff>)"
		R"(<buff skill_ids="31 32 33" pool="1"><restriction random_days_per_month="2"/><trigger condition="ENTER_MAP"/></buff></buffs></event>)");
	std::vector<dao::EventDAO::StoredBuffData> previous;
	previous.emplace_back(0, std::unordered_set<int32_t>{12, 14}, std::unordered_set<int32_t>{2, 4, 6});
	previous.emplace_back(1, std::unordered_set<int32_t>{99}, std::unordered_set<int32_t>{5}); // 99 is no skill of the buff, 1 day is not 2
	ASSERT_TRUE(dao::EventDAO::storeBuffData("Kept", previous));

	runtime::Ref<EventBuffHandler> handler = EventBuffHandler::create("Kept", buffsOf(event));
	std::optional<std::vector<dao::EventDAO::StoredBuffData>> stored = dao::EventDAO::loadStoredBuffData("Kept");
	ASSERT_TRUE(stored);
	ASSERT_EQ(stored->size(), 2u);
	std::sort(stored->begin(), stored->end(), [](const auto& a, const auto& b) { return a.getBuffIndex() < b.getBuffIndex(); });
	EXPECT_EQ((*stored)[0].getActivePoolSkillIds(), (std::unordered_set<int32_t>{12, 14})) << "a pool of the buff's skills is kept";
	EXPECT_EQ((*stored)[0].getAllowedBuffDays(), (std::unordered_set<int32_t>{2, 4, 6})) << "as many days as random_days_per_month: kept";
	ASSERT_TRUE((*stored)[1].getActivePoolSkillIds());
	EXPECT_FALSE((*stored)[1].getActivePoolSkillIds()->contains(99)) << "a pool with a foreign skill is replaced";
	EXPECT_EQ((*stored)[1].getActivePoolSkillIds()->size(), 1u);
	EXPECT_EQ((*stored)[1].getAllowedBuffDays()->size(), 2u) << "a wrong number of days is replaced";
}

TEST_F(EventBuffsTest, ANewDayDrawsANewPool) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: EventBuffHandler reads and writes the event table";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	const model::templates::event::EventTemplate& event = eventTemplate(
		R"(<event name="Daily"><buffs><buff skill_ids="11 12 13 14 15 16 17 18" pool="1"><restriction random_days_per_month="3"/>)"
		R"(<trigger condition="ENTER_MAP"/></buff></buffs></event>)");
	runtime::Ref<EventBuffHandler> handler = EventBuffHandler::create("Daily", buffsOf(event));
	const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());

	ASSERT_TRUE(dao::EventDAO::storeBuffData("Daily", {})) << "empty the stored data";
	handler->onTimeChanged(now);
	EXPECT_TRUE(dao::EventDAO::loadStoredBuffData("Daily")->empty()) << "the same day: nothing is renewed or stored";

	handler->onTimeChanged(now + std::chrono::days(1));
	std::optional<std::vector<dao::EventDAO::StoredBuffData>> stored = dao::EventDAO::loadStoredBuffData("Daily");
	ASSERT_TRUE(stored);
	ASSERT_EQ(stored->size(), 1u) << "a new day: a new pool, stored";
	EXPECT_EQ(stored->front().getActivePoolSkillIds()->size(), 1u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
