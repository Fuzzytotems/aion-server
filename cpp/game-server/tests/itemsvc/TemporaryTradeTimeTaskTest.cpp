// M5c P-04 (m5c-plan.md §5, P5-07): TemporaryTradeTimeTask, the list of the players who may still trade a temporarily tradeable item (a team
// loot whose template has temp_exchange_time: DropService's TempTradeDropPredicate adds it, ExchangeService.addItem asks canTrade), against
// TemporaryTradeTimeTask.java:18-63. Tests of P-06.
//
// The task runs on the fixture's DeterministicExecutor, whose ManualClock starts at wall time 0: AbstractPeriodicTaskManager schedules run() after
// Rnd.get(500, 550) ms and then every 1,000 ms, and run() reads System.currentTimeMillis() as the executor's clock (ThreadPoolManager::clock()).
// The exchange time is set the way TempTradeDropPredicate.changeItem sets it (DropService.java:516-518): (int) (currentTimeMillis / 1000) +
// temp_exchange_time * 60, with Tahabata's Sword's temp_exchange_time="10" (item_templates.xml:4807) and Modor's Tunic's "10"
// (:204906), so 600 s after the start. The expiry message goes to the looters World.getPlayer finds: the fixture's player is stored in the
// World (the Poeta holders of PlayerItemsTestSupport.h), a second looter id is not.

#include "PlayerItemsTestSupport.h"

#include <chrono>
#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/taskmanager/tasks/TemporaryTradeTimeTask.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test::playeritems {
namespace {

using namespace std::chrono_literals;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using taskmanager::tasks::TemporaryTradeTimeTask;

constexpr int32_t OFFLINE_LOOTER = 700999; // a looter World.getPlayer does not find

/** A fresh TemporaryTradeTimeTask: its constructor schedules run() on the installed DeterministicExecutor (500-550 ms, then every 1,000 ms) */
class TestTemporaryTradeTimeTask final : public TemporaryTradeTimeTask {
	AION_MAKE_REF_FRIEND
public:
	TestTemporaryTradeTimeTask() = default;

protected:
	~TestTemporaryTradeTimeTask() override = default;
};

class TemporaryTradeTimeTaskTest : public PlayerItemsTest {
protected:
	void SetUp() override {
		PlayerItemsTest::SetUp();
		publishPoetaCastWorldDataOnce();
		world::World::getInstance().storeObject(player());
		task = runtime::makeRef<TestTemporaryTradeTimeTask>();
	}

	void TearDown() override {
		task = nullptr;
		if (f.player)
			world::World::getInstance().removeObject(*f.player);
		PlayerItemsTest::TearDown();
	}

	/** Java TempTradeDropPredicate.changeItem's exchange time: now in seconds + temp_exchange_time minutes */
	static int32_t exchangeTimeOf(Item& item) {
		return static_cast<int32_t>(utils::ThreadPoolManager::clock().currentTimeMillis() / 1000) + item.getItemTemplate()->getTempExchangeTime() * 60;
	}

	/** The looters of a drop, DropNpc.allowedLooters: the fixture's player (in the World) and one who is not */
	static Ref<runtime::RcHashSet<int32_t>> looters(std::initializer_list<int32_t> ids) {
		Ref<runtime::RcHashSet<int32_t>> set = runtime::RcHashSet<int32_t>::create();
		for (int32_t id : ids)
			set->add(id);
		return set;
	}

	std::vector<uint8_t> timeOver(Item& item) { return serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXCHANGE_TIME_OVER(item.getL10n())); }

	Ref<TestTemporaryTradeTimeTask> task;
};

TEST_F(TemporaryTradeTimeTaskTest, AnItemIsTradeableByItsLootersUntilItsExchangeTimeRunsOut) {
	// TemporaryTradeTimeTask.java:30-39: addTask keeps the looters of the item, canTrade asks them; an item without a task, or a player who is not
	// a looter, cannot trade it. :41-57: every second, an item whose exchange time is reached (time <= 0) sends STR_MSG_EXCHANGE_TIME_OVER to
	// each looter World.getPlayer finds, gets exchange time 0 and leaves the map
	Item& sword = inCube(950001, TAHABATA_SWORD, 1);
	Item& other = inCube(950002, TAHABATA_SWORD, 1);
	sword.setTemporaryExchangeTime(exchangeTimeOf(sword));
	ASSERT_EQ(sword.getTemporaryExchangeTime(), 600);
	Ref<runtime::RcHashSet<int32_t>> allowed = looters({player().getObjectId(), OFFLINE_LOOTER});
	task->addTask(sword, *allowed);

	EXPECT_TRUE(task->canTrade(sword, player().getObjectId()));
	EXPECT_TRUE(task->canTrade(sword, OFFLINE_LOOTER));
	EXPECT_FALSE(task->canTrade(sword, 12345)) << "not a looter";
	EXPECT_FALSE(task->canTrade(other, player().getObjectId())) << "no task for the item";

	// the last run before the time: at 599.5xx s now is 599 s, one second left
	executor->advance(599'999ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(task->canTrade(sword, player().getObjectId()));
	EXPECT_EQ(sword.getTemporaryExchangeTime(), 600);

	// the run at 600.5xx s: now is 600 s, time 0
	executor->advance(601ms);
	EXPECT_EQ(sent(), cp::exactly({timeOver(sword)})) << "only the looter in the World is told";
	EXPECT_EQ(sword.getTemporaryExchangeTime(), 0);
	EXPECT_FALSE(task->canTrade(sword, player().getObjectId()));
	EXPECT_FALSE(task->canTrade(sword, OFFLINE_LOOTER));

	// the entry is gone: no second message
	clearSent();
	executor->advance(5'000ms);
	EXPECT_TRUE(sent().empty());
}

TEST_F(TemporaryTradeTimeTaskTest, TheLootersAreTheCallersOwnSet) {
	// TemporaryTradeTimeTask.java:30-32 stores the collection it is given: DropNpc.startFreeForAll clears allowedLooters in place
	// (DropNpc.java:142-145), after which canTrade answers false for everyone, and a looter added to the set later may trade
	Item& sword = inCube(950101, TAHABATA_SWORD, 1);
	sword.setTemporaryExchangeTime(exchangeTimeOf(sword));
	Ref<runtime::RcHashSet<int32_t>> allowed = looters({player().getObjectId()});
	task->addTask(sword, *allowed);
	ASSERT_TRUE(task->canTrade(sword, player().getObjectId()));

	allowed->clear();
	EXPECT_FALSE(task->canTrade(sword, player().getObjectId()));
	allowed->add(OFFLINE_LOOTER);
	EXPECT_TRUE(task->canTrade(sword, OFFLINE_LOOTER));

	// Map.put: a second addTask of the item replaces its looters
	task->addTask(sword, *looters({player().getObjectId()}));
	EXPECT_FALSE(task->canTrade(sword, OFFLINE_LOOTER));
	EXPECT_TRUE(task->canTrade(sword, player().getObjectId()));
}

TEST_F(TemporaryTradeTimeTaskTest, EachItemExpiresAtItsOwnTime) {
	// TemporaryTradeTimeTask.java:44-56 per entry: the sword runs out at 600 s, the tunic added 60 s later at 660 s, and an item whose time is
	// already over when it is added expires at the first run; each expiry tells the looter once and resets only its own item
	Item& sword = inCube(950201, TAHABATA_SWORD, 1);
	Item& tunic = inCube(950202, MODORS_TUNIC, 1);
	Item& late = inCube(950203, TAHABATA_SWORD, 1);
	sword.setTemporaryExchangeTime(exchangeTimeOf(sword));
	late.setTemporaryExchangeTime(0); // the time is 0 s, the item's time is 0: time <= 0
	task->addTask(sword, *looters({player().getObjectId()}));
	task->addTask(late, *looters({player().getObjectId()}));

	executor->advance(1'000ms); // the first run, at 0.5xx s
	EXPECT_EQ(sent(), cp::exactly({timeOver(late)}));
	EXPECT_FALSE(task->canTrade(late, player().getObjectId()));
	EXPECT_TRUE(task->canTrade(sword, player().getObjectId()));
	clearSent();

	executor->advance(59'000ms); // 60 s
	tunic.setTemporaryExchangeTime(exchangeTimeOf(tunic));
	ASSERT_EQ(tunic.getTemporaryExchangeTime(), 660);
	task->addTask(tunic, *looters({player().getObjectId(), OFFLINE_LOOTER}));

	executor->advance(541'000ms); // 601 s: the sword's run at 600.5xx s
	EXPECT_EQ(sent(), cp::exactly({timeOver(sword)}));
	EXPECT_EQ(sword.getTemporaryExchangeTime(), 0);
	EXPECT_EQ(tunic.getTemporaryExchangeTime(), 660);
	EXPECT_TRUE(task->canTrade(tunic, OFFLINE_LOOTER));
	clearSent();

	executor->advance(59'000ms); // 660 s: the tunic's last run before its time was at 659.5xx s
	EXPECT_TRUE(sent().empty());
	executor->advance(1'000ms);
	EXPECT_EQ(sent(), cp::exactly({timeOver(tunic)}));
	EXPECT_EQ(tunic.getTemporaryExchangeTime(), 0);
	EXPECT_FALSE(task->canTrade(tunic, OFFLINE_LOOTER));
}

TEST_F(TemporaryTradeTimeTaskTest, TheSingletonIsOneTask) {
	// TemporaryTradeTimeTask.java:26-28, 59-62: SingletonHolder
	EXPECT_EQ(&TemporaryTradeTimeTask::getInstance(), &TemporaryTradeTimeTask::getInstance());
}

} // namespace
} // namespace aion::gameserver::services::item::test::playeritems
