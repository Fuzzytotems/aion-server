// M5h-2 H2-1 (m5h-plan.md 14.3): the house auction against HousingBidService.java - register, bid (the refusals in Java's order, an outbid
// player's refund by mail), the end with and without bids, the cancel - and the two housing tasks that feed it, MaintenanceTask's impoundment
// of an overdue house and AuctionAutoFillTask, on the test database aion_gs_test_legionhouse (LegionHouseTestSupport.h, run under gate_lock.py)
// with real Players on recording connections stored in the World (tests/team/P5-10b/TeamTestSupport.h, as LegionServiceTest.cpp).
//
// The land is housing/houses.xml's first Oriel land shape (land 325001, three addresses) with its sale level; the bidding hours depend on the
// server time zone (HousingBidService.isBiddingTime: not Sunday from 12:00), so the fixture sets GSConfig.TIME_ZONE_ID to a zone where the
// auction is open now. The auction mails use mail_templates.xml:93-121 ($$HS_AUCTION_MAIL).

#include "../team/P5-10b/TeamTestSupport.h"
#include "LegionHouseTestSupport.h"

#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/mail/Mails.bind.h"
#include "aion/gameserver/model/templates/mail/Mails.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECEIVE_BIDS.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/taskmanager/tasks/housing/AuctionAutoFillTask.h"
#include "aion/gameserver/taskmanager/tasks/housing/MaintenanceTask.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::taskmanager::tasks::housing {
struct MaintenanceTaskTestAccess {
	static void putHouseToAuction(model::house::House& house, runtime::Ptr<model::gameobjects::player::PlayerCommonData> owner) {
		MaintenanceTask::getInstance().putHouseToAuction(house, std::move(owner));
	}
};
struct AuctionAutoFillTaskTestAccess {
	static void autoFillAuction(model::Race race) { AuctionAutoFillTask::getInstance().autoFillAuction(race); }
};
} // namespace aion::gameserver::taskmanager::tasks::housing

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

namespace lh = ::aion::gameserver::legionhouse::test;
using configs::main::HousingConfig;
using model::house::House;
using model::house::HouseBids;
using runtime::Ptr;
using serverpackets::SM_RECEIVE_BIDS;
using services::HousingBidService;
using services::HousingService;

/**
 * houses.xml's Oriel land 325001 with three of its addresses (sale level 50; a gold price of 1,000,000), their town set to 0: an owner change
 * then builds no Town (TownService, which needs the town spawns and the geo maps of the housing worlds)
 */
constexpr const char* HOUSE_LANDS = R"(<house_lands>
	<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="10001" map="700010000" town="0" x="696.159973" y="1999.969971" z="174.42577"/>
			<address id="10002" map="700010000" town="0" x="609.904907" y="2057.186279" z="174.90712"/>
			<address id="10003" map="700010000" town="0" x="779.445618" y="2061.070068" z="174.19347"/>
		</addresses>
		<buildings>
			<building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/>
		</buildings>
		<sale level="50" gold_price="1000000" point_price="0"/>
		<fee>20000000</fee>
		<caps room="false" floor="false" emblemId="2" addon="true"/>
	</land>
</house_lands>)";

/** mail_templates.xml:93-121, the Elyos template of $$HS_AUCTION_MAIL */
constexpr const char* MAIL_TEMPLATES = R"(<mails><mail name="$$HS_AUCTION_MAIL"><template name="" race="ELYOS"><sender id="903401"/>)"
	R"(<title id="0"><param id="resultid"/><param id="raceid"/></title><header id="0"/>)"
	R"(<body id="0"><param id="datetime"/><param id="address"/><param id="datetime"/></body><tail id="0"/></template></mail></mails>)";

/** npc_templates.xml:530109-530119, the land's manager npc (House.getOwnerName and the appearance read its template) */
constexpr const char* MANAGER_NPC = R"(<npc_templates><npc_template npc_id="810017" level="1" name="palace butler" name_id="461896" height="1.375" )"
	R"(group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" tribe="GENERAL" type="HOUSING" ai="butler" sangle="0" attack_speed="2000" hpgauge="3">)"
	R"(<stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" /></stats>)"
	R"(<equipment><item>110900245</item><item>125003004</item></equipment><bound_radius front="0.7" side="0.444" upper="1.375" />)"
	R"(<talk_info distance="2" is_dialog="true" func_dialogs="88 84 85 86 87 98" can_talk_invisible="false" /></npc_template></npc_templates>)";

constexpr int32_t HOUSE_A = 950001; // address 10001
constexpr int32_t HOUSE_B = 950002; // address 10002
constexpr int32_t HOUSE_C = 950003; // address 10003
constexpr int64_t PRICE = 1000000;   // the land's gold price, House.getDefaultAuctionPrice with HOUSE_MIN_BID 0

/** A time zone in which HousingBidService.isBiddingTime is true now (not Sunday from 12:00): one of the whole-hour Etc zones always is */
const std::chrono::time_zone* openAuctionZone() {
	using namespace std::chrono;
	for (int offset = -12; offset <= 14; offset++) {
		const std::string name = offset == 0 ? "Etc/GMT" : std::format("Etc/GMT{}{}", offset > 0 ? "-" : "+", offset > 0 ? offset : -offset);
		const time_zone* zone = locate_zone(name);
		const local_time<seconds> now = zone->to_local(floor<seconds>(system_clock::now()));
		const local_days today = floor<days>(now);
		if (weekday(today) != Sunday || (now - today) < hours(12))
			return zone;
	}
	return nullptr;
}

const services::cron::CronExpression& auctionEndTime() {
	static const services::cron::CronExpression expression("0 0 12 ? * SUN");
	return expression;
}

class HouseAuctionTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		savedZone = configs::main::GSConfig::TIME_ZONE_ID.exchange(openAuctionZone());
	}

	void TearDown() override {
		for (Member* m : stored)
			world::World::getInstance().removeObject(m->player());
		stored.clear();
		configs::main::GSConfig::TIME_ZONE_ID.store(savedZone);
		TeamTest::TearDown();
	}

	bool requireDatabase() {
		if (!lh::isDatabaseEnabled())
			return false;
		lh::setUpDatabaseOnce();
		// before HousingService's first use in this process: it loads the land houses from these rows
		for (auto [id, address] : {std::pair{HOUSE_A, 10001}, std::pair{HOUSE_B, 10002}, std::pair{HOUSE_C, 10003}})
			lh::execute(std::format("INSERT INTO houses (id, player_id, building_id, address, settings) VALUES ({}, 0, 350000, {}, 0)", id, address));
		return true;
	}

	/** An online Elyos player in the World with a players row, `kinah` in the cube and, if `canOwn`, 18802 COMPLETE (HousingService.canOwnHouse) */
	Member& online(std::string_view name, int64_t kinah, bool canOwn = true) {
		Member& m = addMember(name);
		world::World::getInstance().storeObject(m.player());
		m.player().getCommonData()->setOnline(true); // as PlayerEnterWorldService sets it
		stored.push_back(&m);
		lh::insertPlayer(m.player().getObjectId(), name, m.player().getObjectId());
		if (kinah > 0)
			giveKinah(m, 900000 + static_cast<int32_t>(stored.size()), kinah);
		m.player().setQuestStateList(model::gameobjects::player::QuestStateList::create());
		if (canOwn) {
			runtime::Ref<questEngine::model::QuestState> state = questEngine::model::QuestState::create(18802, questEngine::model::QuestStatus::COMPLETE);
			m.player().getQuestStateList()->addQuest(18802, *state);
		}
		m.clearSent();
		return m;
	}

	static House& house(int32_t id) {
		Ptr<House> found = HousingService::getInstance().findHouse(id);
		EXPECT_TRUE(found) << id;
		return *found;
	}

	/** the query's single value, -1 without a row (a missing mail fails the expectation instead of dereferencing nothing) */
	static int64_t count(std::string_view sql) { return lh::queryLong(sql).value_or(-1); }

	const std::chrono::time_zone* savedZone = nullptr;
	std::vector<Member*> stored;
	lh::PublishedHolder<dataholders::HouseData> houseData{dataholders::DataManager::HOUSE_DATA, lh::bindXml<dataholders::HouseData>(HOUSE_LANDS)};
	lh::PublishedHolder<model::templates::mail::Mails> mails{dataholders::DataManager::SYSTEM_MAIL_TEMPLATES, lh::bindXml<model::templates::mail::Mails>(MAIL_TEMPLATES)};
	// HousingConfig.java:28: the auction end's default cron (AuctionEndTask: a bid asks for the time until the next end)
	ConfigScope<const services::cron::CronExpression*> auctionEnd{HousingConfig::HOUSE_AUCTION_END_TIME, &auctionEndTime()};
	lh::PublishedHolder<dataholders::NpcData> npcs{dataholders::DataManager::NPC_DATA, lh::bindXml<dataholders::NpcData>(MANAGER_NPC)};
	ConfigScope<bool> auctions{HousingConfig::ENABLE_HOUSE_AUCTIONS, true};
	ConfigScope<int32_t> minBidLevel{HousingConfig::HOUSE_MIN_BID_LEVEL, 1};
	ConfigScope<float> stepLimit{HousingConfig::AUCTION_BID_STEP_LIMIT, 100.0f};
	ConfigScope<int32_t> minBid{HousingConfig::HOUSE_MIN_BID, 0};
};

#define AUCTION_REQUIRE_DATABASE()                                                                                                                    \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

TEST_F(HouseAuctionTest, AnAuctionIsRegisteredOnceWithItsInitialOffer) {
	AUCTION_REQUIRE_DATABASE();
	HousingBidService& service = HousingBidService::getInstance();
	House& a = house(HOUSE_A);
	EXPECT_TRUE(service.auction(a, PRICE));
	ASSERT_TRUE(a.getBids()) << "house.setBids(houseBids, true)";
	EXPECT_EQ(service.getBidInfo(a), a.getBids());
	EXPECT_EQ(a.getBids()->getInitialOffer()->getKinah(), PRICE);
	EXPECT_EQ(count(std::format("SELECT COUNT(*) FROM house_bids WHERE player_id = 0 AND house_id = {} AND bid = {}", HOUSE_A, PRICE)), 1)
	  << "HouseBidsDAO.addBid of the initial offer";
	Ptr<HouseBids> registered = a.getBids();
	EXPECT_FALSE(service.auction(a, 2 * PRICE)) << "bids.putIfAbsent: already auctioned";
	EXPECT_EQ(a.getBids(), registered) << "the first auction stays";
	EXPECT_EQ(count(std::format("SELECT COUNT(*) FROM house_bids WHERE house_id = {}", HOUSE_A)), 1) << "no second initial offer";
	EXPECT_EQ(service.getBidInfo(model::Race::ELYOS).size(), 1u);
}

TEST_F(HouseAuctionTest, BidsAreRefusedInJavasOrder) {
	AUCTION_REQUIRE_DATABASE();
	HousingBidService& service = HousingBidService::getInstance();
	ASSERT_TRUE(service.auction(house(HOUSE_A), PRICE));
	const int32_t listIndex = house(HOUSE_A).getBids()->getListIndex();
	Member& noQuest = online("Noquest", 5 * PRICE, false);
	EXPECT_FALSE(service.bid(noQuest.player(), listIndex, PRICE + 1));
	EXPECT_EQ(noQuest.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_OWN_NOT_COMPLETE_QUEST(18802)), 1) << "canOwnHouse(player, true) first";

	Member& bidder = online("Bidder", 5 * PRICE);
	EXPECT_FALSE(service.bid(bidder.player(), listIndex + 1000, PRICE + 1));
	EXPECT_EQ(bidder.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_FAIL()), 1) << "no auction with that list index";
	{
		ConfigScope<int32_t> highLevel{HousingConfig::HOUSE_MIN_BID_LEVEL, 50};
		EXPECT_FALSE(service.bid(bidder.player(), listIndex, PRICE + 1));
		EXPECT_EQ(bidder.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_LOW_LEVEL(50)), 1) << "getMinBidLevel";
	}
	Member& poor = online("Poorbidder", PRICE);
	EXPECT_FALSE(service.bid(poor.player(), listIndex, PRICE + 1));
	EXPECT_EQ(poor.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_NOT_ENOUGH_MONEY(PRICE + 1)), 1);
	EXPECT_FALSE(service.bid(bidder.player(), listIndex, 2 * PRICE)) << "the step limit: a raise of 100% of the current bid";
	EXPECT_EQ(bidder.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_EXCESS_ACCOUNT()), 1);
	EXPECT_EQ(bidder.player().getInventory().getKinah(), 5 * PRICE) << "nothing taken by a refused bid";
}

TEST_F(HouseAuctionTest, ABidTakesTheKinahAndAnOutbidPlayerIsRefundedByMail) {
	AUCTION_REQUIRE_DATABASE();
	HousingBidService& service = HousingBidService::getInstance();
	ASSERT_TRUE(service.auction(house(HOUSE_A), PRICE));
	const int32_t listIndex = house(HOUSE_A).getBids()->getListIndex();
	Member& first = online("Firstbidder", 5 * PRICE);
	Member& second = online("Secondbidder", 5 * PRICE);

	Ptr<HouseBids::Bid> bid = service.bid(first.player(), listIndex, PRICE + 1000);
	ASSERT_TRUE(bid);
	EXPECT_EQ(first.player().getInventory().getKinah(), 5 * PRICE - (PRICE + 1000));
	EXPECT_EQ(first.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_SUCCESS(10001)), 1);
	EXPECT_EQ(first.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_PRICE_CHANGE(PRICE + 1000)), 1);
	EXPECT_EQ(first.count(SM_RECEIVE_BIDS(0)), 1);
	EXPECT_FALSE(service.bid(first.player(), listIndex, PRICE + 2000));
	EXPECT_EQ(first.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CANT_BID_SUCC_BID_HOUSE()), 1) << "the highest bidder bids again";

	first.clearSent();
	ASSERT_TRUE(service.bid(second.player(), listIndex, PRICE + 2000));
	EXPECT_EQ(first.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_CANCEL()), 1) << "the outbid player is told";
	EXPECT_EQ(first.count(SM_RECEIVE_BIDS(0)), 1);
	EXPECT_EQ(count(std::format("SELECT attached_kinah_count FROM mail WHERE mail_recipient_id = {}", first.player().getObjectId())), PRICE + 1000)
	  << "MailFormatter.sendHouseAuctionMail(FAILED_BID, previousBid.getKinah())";
	EXPECT_FALSE(service.bid(second.player(), listIndex, PRICE + 3000));
	EXPECT_EQ(house(HOUSE_A).getBids()->getHighestBid()->getPlayerObjectId(), second.player().getObjectId());
}

TEST_F(HouseAuctionTest, TheEndSellsToTheHighestBidderAndAnUnsoldHouseStaysUnowned) {
	AUCTION_REQUIRE_DATABASE();
	HousingBidService& service = HousingBidService::getInstance();
	ASSERT_TRUE(service.auction(house(HOUSE_A), PRICE));
	ASSERT_TRUE(service.auction(house(HOUSE_B), PRICE));
	Member& buyer = online("Buyer", 5 * PRICE);
	ASSERT_TRUE(service.bid(buyer.player(), house(HOUSE_A).getBids()->getListIndex(), PRICE + 1000));
	buyer.clearSent();

	EXPECT_TRUE(service.endAuction(HOUSE_A));
	EXPECT_EQ(house(HOUSE_A).getOwnerId(), buyer.player().getObjectId()) << "changeOwner(house, buyer)";
	EXPECT_FALSE(house(HOUSE_A).getBids());
	EXPECT_EQ(buyer.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_BID_WIN(10001)), 1);
	EXPECT_EQ(count(std::format("SELECT COUNT(*) FROM house_bids WHERE house_id = {}", HOUSE_A)), 0) << "HouseBidsDAO.deleteHouseBids";
	EXPECT_EQ(count(std::format("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = {}", buyer.player().getObjectId())), 1) << "WIN_BID mail";
	EXPECT_FALSE(service.endAuction(HOUSE_A)) << "no bids any more";

	EXPECT_TRUE(service.endAuction(HOUSE_B)) << "no bidder";
	EXPECT_EQ(house(HOUSE_B).getOwnerId(), 0);
	EXPECT_FALSE(service.getBidInfo(house(HOUSE_B)));
}

TEST_F(HouseAuctionTest, TheWeeklyEndEndsEveryAuction) {
	AUCTION_REQUIRE_DATABASE();
	// AuctionEndTask.executeTask -> HousingBidService.endAuctions (HousingBidService.java:191-197): every auction not prolonged ends
	HousingBidService& service = HousingBidService::getInstance();
	ASSERT_TRUE(service.auction(house(HOUSE_A), PRICE));
	ASSERT_TRUE(service.auction(house(HOUSE_B), PRICE));
	service.endAuctions();
	EXPECT_TRUE(service.getBidInfo(model::Race::ELYOS).empty());
	EXPECT_EQ(count("SELECT COUNT(*) FROM house_bids"), 0);
}

TEST_F(HouseAuctionTest, ACancelledAuctionRefundsTheLastBidder) {
	AUCTION_REQUIRE_DATABASE();
	HousingBidService& service = HousingBidService::getInstance();
	ASSERT_TRUE(service.auction(house(HOUSE_A), PRICE));
	Member& bidder = online("Lastbidder", 5 * PRICE);
	ASSERT_TRUE(service.bid(bidder.player(), house(HOUSE_A).getBids()->getListIndex(), PRICE + 5000));

	EXPECT_TRUE(service.cancelAuction(house(HOUSE_A)));
	EXPECT_FALSE(house(HOUSE_A).getBids());
	EXPECT_EQ(count(std::format("SELECT COUNT(*) FROM house_bids WHERE house_id = {}", HOUSE_A)), 0);
	EXPECT_EQ(count(std::format("SELECT attached_kinah_count FROM mail WHERE mail_recipient_id = {}", bidder.player().getObjectId())), PRICE + 5000)
	  << "CANCELED_BID with the highest bid's kinah";
	EXPECT_FALSE(service.cancelAuction(house(HOUSE_A))) << "nothing to cancel";
}

TEST_F(HouseAuctionTest, MaintenanceImpoundsAnOverdueHouseAndAutoFillAuctionsUnownedHouses) {
	AUCTION_REQUIRE_DATABASE();
	Member& owner = online("Overdue", 0);
	HousingService::getInstance().changeOwner(house(HOUSE_C), owner.player().getObjectId());
	owner.clearSent();
	// MaintenanceTask.putHouseToAuction (MaintenanceTask.java:86-92): the owner loses it, it goes to auction at its default price
	taskmanager::tasks::housing::MaintenanceTaskTestAccess::putHouseToAuction(house(HOUSE_C), owner.player().getCommonData());
	EXPECT_EQ(house(HOUSE_C).getOwnerId(), 0);
	ASSERT_TRUE(house(HOUSE_C).getBids());
	EXPECT_EQ(house(HOUSE_C).getBids()->getInitialOffer()->getKinah(), PRICE);
	EXPECT_EQ(owner.count(SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_SEQUESTRATE()), 1);

	// AuctionAutoFillTask: the Elyos houses without owner and auction, up to the limit per house type
	auto savedLimits = HousingConfig::AUCTION_AUTO_FILL_LIMITS.get();
	HousingConfig::AUCTION_AUTO_FILL_LIMITS.set(std::map<configs::detail::HouseType, int32_t>{{configs::detail::HouseType::HOUSE, 2}});
	taskmanager::tasks::housing::AuctionAutoFillTaskTestAccess::autoFillAuction(model::Race::ELYOS);
	EXPECT_EQ(HousingBidService::getInstance().getBidInfo(model::Race::ELYOS).size(), 2u) << "C already auctioned counts against the limit of 2";
	EXPECT_EQ(static_cast<int>(static_cast<bool>(house(HOUSE_A).getBids())) + static_cast<int>(static_cast<bool>(house(HOUSE_B).getBids())), 1);
	if (savedLimits)
		HousingConfig::AUCTION_AUTO_FILL_LIMITS.set(*savedLimits);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
