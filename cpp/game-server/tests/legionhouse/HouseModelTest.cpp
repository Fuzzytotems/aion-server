// The P5-11 housing model (m5a-plan.md E2-05): House construction and its door, permission and persistent state rules, the house registry
// without registered objects, HouseBids with the bid and bid-removal rules, the HouseDoorState companion and the deactivated AuctionEndTask
// singleton.
//
// Expectations are derived by hand from House.java, HouseRegistry.java, HouseBids.java, HouseDoorState.java and AbstractCronTask.java:31-35.

#include <gtest/gtest.h>

#include <vector>

#include "LegionHouseTestSupport.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/house/HouseDoorStateInfo.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/taskmanager/tasks/housing/AuctionEndTask.h"

namespace aion::gameserver::legionhouse::test {
namespace {

using model::gameobjects::Persistable;
using model::house::House;
using model::house::HouseBids;
using model::house::HouseDoorState;

/** Sets an atomic configuration field for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

constexpr std::string_view HOUSE_LANDS = R"(<house_lands>
	<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/>
			<address id="10002" map="700010000" town="1001" x="609.904907" y="2057.186279" z="174.90712"/>
		</addresses>
		<buildings>
			<building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/>
			<building id="350001" type="PERSONAL_FIELD" size="MANSION"/>
		</buildings>
		<sale level="50" gold_price="1000000000" point_price="0"/>
		<fee>20000000</fee>
		<caps room="false" floor="false" emblemId="2" addon="true"/>
	</land>
</house_lands>)";

class HouseModelTest : public LegionHouseTest {
protected:
	void SetUp() override {
		LegionHouseTest::SetUp();
		houseData.emplace(dataholders::DataManager::HOUSE_DATA, bindXml<dataholders::HouseData>(HOUSE_LANDS));
	}

	void TearDown() override {
		houseData.reset();
		LegionHouseTest::TearDown();
	}

	static const model::templates::housing::HouseAddress* address(int32_t id) { return dataholders::DataManager::HOUSE_DATA->getAddress(id); }

	std::optional<PublishedHolder<dataholders::HouseData>> houseData;
};

TEST_F(HouseModelTest, NewHouseUsesTheDefaultBuildingAndStartsClosedAndUpdated) {
	runtime::Ref<House> house = model::gameobjects::VisibleObject::create<House>(address(10001), 0);
	ASSERT_TRUE(house->getAddress());
	EXPECT_EQ(house->getBuilding()->getId(), 350000) << "HousingLand.getDefaultBuilding()";
	EXPECT_EQ(house->getName(), "HOUSE_10001");
	EXPECT_EQ(house->getLand()->getId(), 325001);
	// resetDoorState(): no owner and no bids -> CLOSED; the constructor ends with setPersistentState(UPDATED)
	EXPECT_EQ(house->getDoorState(), HouseDoorState::CLOSED);
	EXPECT_EQ(house->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_TRUE(house->isShowOwnerName());
	EXPECT_FALSE(house->getOwnerName()) << "null without owner";
	EXPECT_EQ(house->getHouseType(), model::templates::housing::HouseType::HOUSE);
	EXPECT_TRUE(house->isFeePaid()) << "no next payment date";
	EXPECT_EQ(house->secondsUntilGraceEnd(), -1) << "an active house";
}

TEST_F(HouseModelTest, PermissionsRoundTripAndInvalidDoorStates) {
	runtime::Ref<House> house = model::gameobjects::VisibleObject::create<House>(4711, address(10002)->getLand()->getDefaultBuilding(), address(10002), 0);

	house->setPermissionsFromDB(0x0201); // show owner name, CLOSED_EXCEPT_FRIENDS (2)
	EXPECT_TRUE(house->isShowOwnerName());
	EXPECT_EQ(house->getDoorState(), HouseDoorState::CLOSED_EXCEPT_FRIENDS);
	EXPECT_EQ(house->getPermissionsForDB(), 0x0201);
	EXPECT_EQ(house->getPersistentState(), Persistable::PersistentState::UPDATED) << "the loading setter changes no state";

	house->setPermissionsFromDB(0x0900); // unknown door state 9 -> null -> resetDoorState() (no owner: CLOSED), which requests an update
	EXPECT_FALSE(house->isShowOwnerName());
	EXPECT_EQ(house->getDoorState(), HouseDoorState::CLOSED);
	EXPECT_EQ(house->getPermissionsForDB(), 0x0300);
	EXPECT_EQ(house->getPersistentState(), Persistable::PersistentState::UPDATE_REQUIRED);

	house->setOwnerId(100);
	EXPECT_TRUE(house->resetDoorState()) << "an owned active house opens";
	EXPECT_EQ(house->getDoorState(), HouseDoorState::OPEN);
	EXPECT_FALSE(house->resetDoorState()) << "no change";
}

TEST_F(HouseModelTest, PersistentStateAndOwnerStates) {
	using model::gameobjects::player::HouseOwnerState;
	runtime::Ref<House> house = model::gameobjects::VisibleObject::create<House>(address(10001), 0);

	house->setPersistentState(Persistable::PersistentState::NEW);
	house->setOwnerId(0); // UPDATE_REQUIRED keeps NEW
	EXPECT_EQ(house->getPersistentState(), Persistable::PersistentState::NEW);
	house->setPersistentState(Persistable::PersistentState::DELETED);
	EXPECT_EQ(house->getPersistentState(), Persistable::PersistentState::NOACTION) << "a new house that is deleted needs no DB action";
	house->setPersistentState(Persistable::PersistentState::DELETED);
	EXPECT_EQ(house->getPersistentState(), Persistable::PersistentState::DELETED);

	EXPECT_EQ(house->getHouseOwnerStates(), 2) << "SINGLE_HOUSE without owner and bids";
	house->setOwnerId(100);
	EXPECT_EQ(house->getHouseOwnerStates(), 1 | 4) << "HAS_OWNER | BIDDING_ALLOWED";
	house->setInactive(true);
	EXPECT_EQ(house->getHouseOwnerStates(), 4) << "BIDDING_ALLOWED";

	AtomicConfigScope<int32_t> mansionMinBid(configs::main::HousingConfig::MANSION_MIN_BID, 0);
	EXPECT_EQ(house->getDefaultAuctionPrice(), 1000000000) << "the land's gold price for a HOUSE without configured minimum bid";
	AtomicConfigScope<int32_t> houseMinBid(configs::main::HousingConfig::HOUSE_MIN_BID, 5000);
	EXPECT_EQ(house->getDefaultAuctionPrice(), 5000);
}

TEST_F(HouseModelTest, RegistryWithoutObjects) {
	runtime::Ref<House> house = model::gameobjects::VisibleObject::create<House>(address(10001), 0);
	runtime::Ref<model::house::HouseRegistry> registry = model::house::HouseRegistry::create(*house);
	EXPECT_EQ(registry->size(), 0);
	EXPECT_TRUE(registry->getObjects().empty());
	EXPECT_TRUE(registry->getSpawnedObjects().empty());
	EXPECT_TRUE(registry->getUnusedDecors().empty());
	EXPECT_FALSE(registry->getObjectByObjId(1));
	EXPECT_EQ(registry->getPersistentState(), Persistable::PersistentState::UPDATED);
	EXPECT_NO_THROW(registry->save()) << "UPDATED: no DAO call";
}

TEST(HouseBidsTest, BiddingRulesFeesAndRemovalOfDeletedPlayers) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	AtomicConfigScope<float> registrationFee(configs::main::HousingConfig::AUCTION_REGISTRATION_FEE_PERCENT, 0.3f);
	AtomicConfigScope<float> salesCommission(configs::main::HousingConfig::AUCTION_SALES_COMMISION_PERCENT, 0.1f);

	runtime::Ref<HouseBids> bids = HouseBids::create(4711, 1000, 111);
	runtime::Ref<HouseBids> otherBids = HouseBids::create(4712, 50);
	EXPECT_EQ(otherBids->getListIndex(), bids->getListIndex() + 1);
	EXPECT_EQ(bids->getHouseObjectId(), 4711);
	EXPECT_EQ(bids->getBidCount(), 0);
	ASSERT_TRUE(bids->getInitialOffer());
	EXPECT_EQ(bids->getInitialOffer()->getKinah(), 1000);
	EXPECT_EQ(bids->getInitialOffer()->getTime(), 111);
	EXPECT_EQ(bids->getInitialOffer()->getPlayerObjectId(), 0);

	EXPECT_TRUE(bids->bid(7, 1000, 200)) << "the initial price may be matched";
	EXPECT_FALSE(bids->bid(8, 1000, 201)) << "a player bid must be exceeded";
	runtime::Ptr<HouseBids::Bid> bid8 = bids->bid(8, 1001, 202);
	ASSERT_TRUE(bid8);
	EXPECT_TRUE(bids->bid(7, 1500, 203));
	EXPECT_EQ(bids->getBidCount(), 3);

	// (long) (1001 * 0.1f) = 100; kinah - commission + (long) (1000 * 0.3f) = 1001 - 100 + 300
	EXPECT_EQ(bid8->calculateSalesCommission(), 100);
	EXPECT_EQ(bid8->calculateSaleRewardKinah(), 1201);

	// player 7: bid 1 (index 1) is deleted, the highest bid (index 3) is kept with bidder 0
	std::vector<runtime::Ref<HouseBids::Bid>> deleted = bids->deleteOrDisableBids(7);
	ASSERT_EQ(deleted.size(), 1u);
	EXPECT_EQ(deleted[0]->getKinah(), 1000);
	EXPECT_EQ(bids->getBidCount(), 2);
	EXPECT_EQ(bids->getHighestBid()->getKinah(), 1500);
	EXPECT_EQ(bids->getHighestBid()->getPlayerObjectId(), 0);
	EXPECT_TRUE(bids->deleteOrDisableBids(9).empty());
}

TEST(HouseDoorStateInfoTest, IdsAndLookup) {
	EXPECT_EQ(model::house::getId(HouseDoorState::OPEN), 1);
	EXPECT_EQ(model::house::getId(HouseDoorState::CLOSED_EXCEPT_FRIENDS), 2);
	EXPECT_EQ(model::house::getId(HouseDoorState::CLOSED), 3);
	EXPECT_EQ(model::house::houseDoorStateOf(3), HouseDoorState::CLOSED);
	EXPECT_EQ(model::house::houseDoorStateOf(0), std::nullopt);
	EXPECT_EQ(model::house::houseDoorStateOf(4), std::nullopt);
}

TEST_F(HouseModelTest, AuctionEndTaskWithoutCronExpressionIsDeactivated) {
	AtomicConfigScope<const services::cron::CronExpression*> noEndTime(configs::main::HousingConfig::HOUSE_AUCTION_END_TIME, nullptr);
	taskmanager::tasks::housing::AuctionEndTask& task = taskmanager::tasks::housing::AuctionEndTask::getInstance();
	EXPECT_FALSE(task.getNextRun()) << "AbstractCronTask: a null cron expression deactivates the task";
	EXPECT_FALSE(task.getLastRun());
	EXPECT_FALSE(task.isAuctionProlonged(4711));
	EXPECT_EQ(executor->pendingTaskCount(), 0u) << "nothing is scheduled";
}

} // namespace
} // namespace aion::gameserver::legionhouse::test
