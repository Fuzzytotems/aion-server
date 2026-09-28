// M5c T-02 / T-04 (m5c-plan.md §5, P5-09b): ExchangeService's state machine between two players - registering, adding kinah and items (the
// whole stack, the split copy, the refusals, the staff restriction of AdminService, the temporary trade time of W-07), locking, the two
// confirmations and the trade with its two inventory writes (D11), the refused validation of either side, an item gone before the trade,
// cancel and logout, the no-duplication invariant, the object ids (IDFactory) of the split copies, and the double-confirm race of D7 on the
// fixture's DeterministicExecutor.
//
// Java: ExchangeService.java:43-346. A completed trade calls InventoryDAO.store for both players (:259-260); the DAO logs and swallows an
// SQLException, so without the test database only the case that reads the rows back is skipped.
//
// D7 (m5c-plan.md §4, §19.4): each player's CM_EXCHANGE_OK is a task; `confirm()` (:225) and the partner's `isConfirmed()` (:230) are a
// check-then-act on plain fields, so in Java two tasks running at once can both pass the check and both trade. T-04 measured what that costs
// (split stacks destroyed, the kinah paid twice each way, the object id of a live item released); the owner chose on 2026-09-27 to fix it
// (Deviation: D7, docs/deviations/P5-09b.md): the two confirmations of one pair are serialized, so the trade runs once. ExchangeRaceTest shows
// it closed on the fixture's DeterministicExecutor in two ways:
// - T-04's measured interleaving, for its four offer mixes: B's CM_EXCHANGE_OK is queued after its first statement (the test runs
//   `Exchange.confirm()` of B's exchange, which is all :225 does) and runs at a named point inside A's performTrade, the exchange log line
//   Java writes right after it put A's first item into B's cube (:336-338, LoggingConfig.LOG_PLAYER_EXCHANGE on): "both players passed :230;
//   B's confirmation runs after A removed both players' items (:248) and before A stored and cleaned up (:259-262)".
// - The sweeps (one context switch): one partner's whole CM_EXCHANGE_OK runs at every interleaving point of the other's (InterleavingSweep
//   below), both ways round, with and without the inner one's :225 already run, and each result is checked against the unraced trade.
// The sweeps and TheTradeGivesThePairsKeyBack (the two unraced orders) check that the pair's key is given back: after the trade only the test
// holds the pair's two Exchange objects.

#include "TradeTestSupport.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/taskmanager/tasks/TemporaryTradeTimeTask.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::economy::test::trade {
namespace {

using model::trade::Exchange;
using network::aion::serverpackets::SM_EXCHANGE_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::ExchangeService;
using services::item::ItemPacketService_ItemAddType;
using services::item::ItemPacketService_ItemUpdateType;
using utils::idfactory::IDFactory;
using PersistentState = model::gameobjects::Persistable_PersistentState;

const char* AUDIT_LOGGER = "AUDIT_LOG";

// A's items
constexpr int32_t A_POTIONS = 900001; // 100 x Minor Life Potion
constexpr int32_t A_SWORD = 900002;   // 1 x Training Sword
constexpr int32_t A_JUICE = 900003;   // 12 x Mercenary's Fruit Juice (not tradeable)
constexpr int32_t A_KINAH = 900009;   // 1,000 kinah
// B's items
constexpr int32_t B_POTIONS = 900101; // 50 x Minor Life Potion
constexpr int32_t B_SWORD = 900102;   // 1 x Training Sword
constexpr int32_t B_KINAH = 900109;   // 500 kinah

class ExchangeTest : public TradeTest {
protected:
	void SetUp() override {
		TradeTest::SetUp();
		stockUp();
	}

	/** Both cubes as every case starts them */
	void stockUp() {
		give(a(), A_POTIONS, MINOR_LIFE_POTION, 100);
		give(a(), A_SWORD, TRAINING_SWORD, 1);
		give(a(), A_JUICE, FRUIT_JUICE, 12);
		setKinah(a(), A_KINAH, 1000);
		give(partner(), B_POTIONS, MINOR_LIFE_POTION, 50);
		give(partner(), B_SWORD, TRAINING_SWORD, 1);
		setKinah(partner(), B_KINAH, 500);
	}

	/** Ends any exchange of the two and empties both cubes (Storage.remove: no packet), then stocks them up again */
	void restock() {
		for (Player* player : {&a(), &partner()}) {
			if (service().isPlayerInExchange(*player))
				service().cancelExchange(*player);
			for (const runtime::Ptr<Item>& item : player->getInventory().getItemsWithKinah())
				if (item->getItemId() != KINAH)
					player->getInventory().remove(*item);
		}
		stockUp(); // the kinah items are replaced
		clearSent();
		clearSentB();
	}

	ExchangeService& service() { return ExchangeService::getInstance(); }

	void start() {
		service().registerExchange(a(), partner());
		ASSERT_TRUE(a().isTrading());
		ASSERT_TRUE(partner().isTrading());
		clearSent();
		clearSentB();
	}

	/** A's own exchange (the partner's exchange of B) */
	runtime::Ptr<Exchange> exchangeOf(Player& player) { return service().getCurrentParnterExchange(&player == &a() ? partner() : a()); }

	void confirm(Player& player) { service().confirmExchange(runtime::Ptr<Player>(player)); }

	/** The inventory, per object id: {item id, count} */
	static std::map<int32_t, std::pair<int32_t, int64_t>> inventoryOf(Player& player) {
		std::map<int32_t, std::pair<int32_t, int64_t>> items;
		for (const runtime::Ptr<Item>& item : player.getInventory().getItemsWithKinah())
			items[item->getObjectId()] = {item->getItemId(), item->getItemCount()};
		return items;
	}

	/** Per item id, the count over both inventories (kinah included) */
	std::map<int32_t, int64_t> totals() {
		std::map<int32_t, int64_t> sums;
		for (Player* player : {&a(), &partner()})
			for (const auto& [objId, idCount] : inventoryOf(*player))
				sums[idCount.first] += idCount.second;
		return sums;
	}

	/** The no-duplication invariant's second half: no object id is in both inventories */
	void expectNoSharedObject() {
		std::map<int32_t, std::pair<int32_t, int64_t>> mine = inventoryOf(a());
		for (const auto& [objId, idCount] : inventoryOf(partner()))
			EXPECT_FALSE(mine.contains(objId)) << "object " << objId << " is in both inventories";
	}

	/** A offers his whole potion stack and 300 kinah; B his sword and 20 of his 50 potions (a split copy) */
	void offerMixed() {
		service().addItem(a(), A_POTIONS, 100);
		service().addKinah(a(), 300);
		service().addItem(partner(), B_SWORD, 1);
		service().addItem(partner(), B_POTIONS, 20);
		clearSent();
		clearSentB();
	}

	/** After offerMixed and a completed trade (ExchangeService.java:280-346) */
	void expectMixedTradeDone() {
		EXPECT_FALSE(a().isTrading());
		EXPECT_FALSE(partner().isTrading());
		EXPECT_EQ(kinah(a()), 1000 - 300);
		EXPECT_EQ(kinah(partner()), 500 + 300);
		ASSERT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS)) << "a whole stack moves as the same object";
		EXPECT_EQ(partner().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 100);
		EXPECT_FALSE(a().getInventory().getItemByObjId(A_POTIONS));
		EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
		EXPECT_FALSE(partner().getInventory().getItemByObjId(B_SWORD));
		EXPECT_EQ(partner().getInventory().getItemByObjId(B_POTIONS)->getItemCount(), 30) << "the split's rest stays";
		EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 20) << "the split copy (a new object) went to A";
		EXPECT_FALSE(a().getInventory().getItemByObjId(B_POTIONS));
		EXPECT_EQ(countOf(a(), TRAINING_SWORD), 2);
		EXPECT_EQ(countOf(partner(), TRAINING_SWORD), 0);
		// the no-duplication invariant: every item id's total over both players is conserved and no object is in both inventories
		EXPECT_EQ(totals(), (std::map<int32_t, int64_t>{{KINAH, 1500}, {TRAINING_SWORD, 2}, {MINOR_LIFE_POTION, 150}, {FRUIT_JUICE, 12}}));
		expectNoSharedObject();
	}
};

// :43-52
TEST_F(ExchangeTest, RegisteringStartsTheExchangeOfBothAndSendsEachTheOthersName) {
	service().registerExchange(a(), partner());

	EXPECT_TRUE(a().isTrading());
	EXPECT_TRUE(partner().isTrading());
	EXPECT_EQ(sent(), cp::exactly({exchangeRequest("Partner")}));
	EXPECT_EQ(sentB(), cp::exactly({exchangeRequest("Holder")}));
	runtime::Ptr<Exchange> ofB = service().getCurrentParnterExchange(a());
	ASSERT_TRUE(ofB);
	EXPECT_EQ(ofB->getActiveplayer().rawPointer(), &partner());
	EXPECT_EQ(ofB->getTargetPlayer().rawPointer(), &a());
	EXPECT_EQ(service().getCurrentParnterExchange(partner())->getActiveplayer().rawPointer(), &a());
}

// :54-56: PlayerRestrictions.canTrade of both
TEST_F(ExchangeTest, NobodyIsRegisteredWhenOneOfThemCannotTrade) {
	partner().setLifeStats(std::make_unique<cp::DeadPlayerLifeStats>(partner()));
	service().registerExchange(a(), partner());
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());
}

TEST_F(ExchangeTest, ASecondRequestDuringAnExchangeIsRefusedAndChangesNothing) {
	start();
	runtime::Ptr<Exchange> ofA = exchangeOf(a());

	service().registerExchange(a(), partner());

	// W-28: PlayerRestrictions.canTrade's isTrading() arm (PlayerRestrictions.java:247-249)
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_EXCHANGE_PARTNER_IS_EXCHANGING_WITH_OTHER())}));
	EXPECT_EQ(exchangeOf(a()).rawPointer(), ofA.rawPointer()) << "the running exchange is kept";
}

// :76-98: never more than the kinah not yet offered; the partner sees each addition
TEST_F(ExchangeTest, KinahIsClampedToWhatIsNotYetOffered) {
	start();

	service().addKinah(a(), 300);
	EXPECT_EQ(sent(), cp::exactly({exchangeAddKinah(300, 0)}));
	EXPECT_EQ(sentB(), cp::exactly({exchangeAddKinah(300, 1)}));
	EXPECT_EQ(exchangeOf(a())->getKinahCount(), 300);

	clearSent();
	clearSentB();
	service().addKinah(a(), 900); // 1,000 - 300 = 700 are left
	EXPECT_EQ(sent(), cp::exactly({exchangeAddKinah(700, 0)}));
	EXPECT_EQ(sentB(), cp::exactly({exchangeAddKinah(700, 1)}));
	EXPECT_EQ(exchangeOf(a())->getKinahCount(), 1000);

	clearSent();
	clearSentB();
	service().addKinah(a(), 1); // nothing left
	service().addKinah(a(), 0);
	service().addKinah(a(), -5);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());
	EXPECT_EQ(exchangeOf(a())->getKinahCount(), 1000);
	EXPECT_EQ(kinah(a()), 1000) << "offering takes nothing yet";
}

TEST_F(ExchangeTest, NoKinahIsAddedToALockedOrMissingExchange) {
	service().addKinah(a(), 10);
	EXPECT_TRUE(sent().empty()) << "no exchange";
	start();
	service().lockExchange(a());
	clearSentB();
	service().addKinah(a(), 10);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());
	EXPECT_EQ(exchangeOf(a())->getKinahCount(), 0);
}

// :100-171: the whole stack leaves the player's view (SM_DELETE_ITEM PUT_TO_EXCHANGE); a part of a stack is a split copy, and the player sees a
// fake item with the rest (SM_INVENTORY_UPDATE_ITEM PUT_TO_EXCHANGE); each addition is shown to both
TEST_F(ExchangeTest, AWholeStackLeavesTheViewAndAPartOfAStackIsASplitCopy) {
	start();

	service().addItem(a(), A_SWORD, 1);
	runtime::Ptr<model::trade::ExchangeItem> sword = exchangeOf(a())->getItems().get(A_SWORD);
	ASSERT_TRUE(sword);
	EXPECT_EQ(sword->getItem().rawPointer(), a().getInventory().getItemByObjId(A_SWORD).rawPointer()) << ":141-142 the item itself";
	EXPECT_EQ(sent(), cp::exactly({deleteItem(A_SWORD, DELETE_PUT_TO_EXCHANGE), serializedFor(SM_EXCHANGE_ADD_ITEM(0, *sword->getItem(), a()))}));
	EXPECT_EQ(sentB(), cp::exactly({serializedForB(SM_EXCHANGE_ADD_ITEM(1, *sword->getItem(), partner()))}));

	clearSent();
	clearSentB();
	service().addItem(a(), A_POTIONS, 20);
	runtime::Ptr<model::trade::ExchangeItem> potions = exchangeOf(a())->getItems().get(A_POTIONS);
	ASSERT_TRUE(potions);
	EXPECT_NE(potions->getItem()->getObjectId(), A_POTIONS) << ":139-140 ItemFactory.newItem: a copy with its own id";
	EXPECT_EQ(potions->getItem()->getItemCount(), 20);
	EXPECT_EQ(potions->getItemObjId(), A_POTIONS);
	runtime::Ref<Item> fake = Item::create(A_POTIONS, a().getInventory().getItemByObjId(A_POTIONS)->getItemTemplate());
	fake->setItemCount(80);
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_INVENTORY_UPDATE_ITEM(a(), *fake, ItemPacketService_ItemUpdateType::PUT_TO_EXCHANGE)),
						  serializedFor(SM_EXCHANGE_ADD_ITEM(0, *potions->getItem(), a()))}));
	EXPECT_EQ(sentB(), cp::exactly({serializedForB(SM_EXCHANGE_ADD_ITEM(1, *potions->getItem(), partner()))}));

	// :148-158: a second addition grows the copy, never beyond the stack; the full stack leaves the view
	clearSent();
	service().addItem(a(), A_POTIONS, 30);
	EXPECT_EQ(potions->getItemCount(), 50);
	EXPECT_EQ(potions->getItem()->getItemCount(), 50) << "ExchangeItem.addCount sets the copy's count";
	fake->setItemCount(50);
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_INVENTORY_UPDATE_ITEM_OPCODE),
		cp::exactly({serializedFor(SM_INVENTORY_UPDATE_ITEM(a(), *fake, ItemPacketService_ItemUpdateType::PUT_TO_EXCHANGE))}));
	clearSent();
	service().addItem(a(), A_POTIONS, 80); // 50 are left
	EXPECT_EQ(potions->getItemCount(), 100);
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_DELETE_ITEM_OPCODE), cp::exactly({deleteItem(A_POTIONS, DELETE_PUT_TO_EXCHANGE)}));
	clearSent();
	clearSentB();
	service().addItem(a(), A_POTIONS, 1);
	EXPECT_TRUE(sent().empty()) << ":152-153 everything is offered already";
	EXPECT_TRUE(sentB().empty());
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100) << "offering takes nothing yet";
}

// :100-131: the refusals, each silent
TEST_F(ExchangeTest, TheRefusalsOfAddItem) {
	service().addItem(a(), A_SWORD, 1);
	EXPECT_TRUE(sent().empty()) << ":105-107 no partner: no exchange";
	start();

	service().addItem(a(), 999999, 1);       // :101-103 not in the inventory
	service().addItem(a(), B_SWORD, 1);      // the partner's item
	service().addItem(a(), A_JUICE, 1);      // :108-111 not tradeable, no temporary trade time, no legion
	service().addItem(a(), A_POTIONS, 0);    // :113-114
	service().addItem(a(), A_POTIONS, -1);
	service().addItem(a(), A_POTIONS, 101);  // :116-117 more than the stack
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());
	EXPECT_TRUE(exchangeOf(a())->getItems().isEmpty());

	// :124-125 locked
	service().lockExchange(a());
	clearSentB();
	service().addItem(a(), A_POTIONS, 1);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(exchangeOf(a())->getItems().isEmpty());
}

// :130-131 (AdminService.java:57-74): a staff member whose access level is below gameserver.administration.unrestricted_itemtrade offers no
// item to a normal player and is told so. The shipped value is 1 (config/administration/admin.properties:27), which every staff member
// reaches; the refusal needs a profile with a higher value, here 3 against A's access level 1. AdminService's list of items staff may hand
// out (config/administration/item.restriction.txt) holds none of the fixture's items.
TEST_F(ExchangeTest, AStaffMemberBelowUnrestrictedItemTradeOffersNoItemToANormalPlayer) {
	ConfigScope<int8_t> unrestrictedItemTrade(configs::administration::AdminConfig::UNRESTRICTED_ITEMTRADE, int8_t{3});
	f.account->setAccessLevel(1);
	start();
	const std::vector<uint8_t> refusal = serializedFor(
		network::aion::serverpackets::SM_MESSAGE(0, "", "You cannot use trade with this item.", model::ChatType::GOLDEN_YELLOW));

	service().addItem(a(), A_SWORD, 1);

	EXPECT_TRUE(exchangeOf(a())->getItems().isEmpty());
	EXPECT_EQ(sent(), cp::exactly({refusal})) << "AdminService.java:72";
	EXPECT_TRUE(sentB().empty());

	// the controls: a partner who is staff too (AdminService.java:64-65), and the shipped unrestricted_itemtrade = 1 (:61-62)
	b.account->setAccessLevel(1);
	clearSent();
	service().addItem(a(), A_SWORD, 1);
	EXPECT_TRUE(exchangeOf(a())->getItems().get(A_SWORD)) << "between staff members";
	EXPECT_EQ(std::ranges::count(sent(), refusal), 0);

	b.account->setAccessLevel(0);
	configs::administration::AdminConfig::UNRESTRICTED_ITEMTRADE.store(1);
	clearSent();
	service().addItem(a(), A_POTIONS, 10);
	EXPECT_TRUE(exchangeOf(a())->getItems().get(A_POTIONS)) << "access level 1 reaches the shipped unrestricted_itemtrade";
	EXPECT_EQ(std::ranges::count(sent(), refusal), 0);
	f.account->setAccessLevel(0);
}

TEST_F(ExchangeTest, EighteenItemsFillTheList) {
	start();
	for (int32_t i = 0; i < 19; i++)
		give(a(), 901000 + i, TRAINING_SWORD, 1);
	for (int32_t i = 0; i < 18; i++)
		service().addItem(a(), 901000 + i, 1);
	ASSERT_EQ(exchangeOf(a())->getItems().size(), 18);
	clearSent();
	clearSentB();

	service().addItem(a(), 901018, 1); // Exchange.isExchangeListFull: size >= 18 (:127-128)

	EXPECT_EQ(exchangeOf(a())->getItems().size(), 18);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());
}

// :108-111 (W-07): an untradeable item may be traded while it is packed or while TemporaryTradeTimeTask lists the partner for it
TEST_F(ExchangeTest, AnUntradeableItemIsOfferedWhilePackedOrWithinItsTemporaryTradeTime) {
	start();
	runtime::Ref<runtime::RcHashSet<int32_t>> others = runtime::RcHashSet<int32_t>::create(AION_LOCK_CLASS(ExchangeTest::others));
	others->add(424242);
	taskmanager::tasks::TemporaryTradeTimeTask::getInstance().addTask(*a().getInventory().getItemByObjId(A_JUICE), *others);
	service().addItem(a(), A_JUICE, 1);
	EXPECT_FALSE(exchangeOf(a())->getItems().get(A_JUICE)) << "the task lists other players only";

	others->add(partner().getObjectId());
	service().addItem(a(), A_JUICE, 1);
	EXPECT_TRUE(exchangeOf(a())->getItems().get(A_JUICE)) << "TemporaryTradeTimeTask.canTrade(item, partner)";

	Item& packed = give(a(), 900004, FRUIT_JUICE, 1);
	packed.setPackCount(1);
	service().addItem(a(), 900004, 1);
	EXPECT_TRUE(exchangeOf(a())->getItems().get(900004)) << "getPackCount() > 0";

	// clean-up: TemporaryTradeTimeTask.run ends the juice's exchange time (0: over) at its first period, so the singleton lets go of the item
	executor->advance(std::chrono::milliseconds(1000));
}

// :173-180
TEST_F(ExchangeTest, LockingTellsThePartner) {
	service().lockExchange(a());
	EXPECT_TRUE(sentB().empty()) << "no exchange";
	start();

	service().lockExchange(a());

	EXPECT_TRUE(exchangeOf(a())->isLocked());
	EXPECT_FALSE(exchangeOf(partner())->isLocked());
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(sentB(), cp::exactly({exchangeConfirmation(3)}));
}

// :216-233: the first confirmation only tells the partner; the second trades (:235-263)
TEST_F(ExchangeTest, TheSecondConfirmationTradesAndCleansUp) {
	start();
	offerMixed();

	confirm(a());
	EXPECT_TRUE(exchangeOf(a())->isConfirmed());
	EXPECT_EQ(sentB(), cp::exactly({exchangeConfirmation(2)}));
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(a().isTrading()) << "the partner has not confirmed";
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);

	clearSentB();
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
	confirm(partner());

	expectMixedTradeDone();
	// :262 cleanUpExchanges(false, ...): the split copy B traded keeps its id in A's cube - nothing is released
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined);
	std::vector<runtime::Ptr<Item>> copies = a().getInventory().getItemsByItemId(MINOR_LIFE_POTION);
	ASSERT_EQ(copies.size(), 1u);
	if constexpr (runtime::CHECKED)
		EXPECT_EQ(IDFactory::getInstance().recentlyReleased(copies[0]->getObjectId()), nullptr);
	// B: SM_EXCHANGE_CONFIRMATION(2) went to A first (:228), then (0) to both (:254-255) before the items arrive
	std::vector<std::vector<uint8_t>> toA = ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE);
	EXPECT_EQ(toA, cp::exactly({exchangeConfirmation(2), exchangeConfirmation(0)}));
	EXPECT_EQ(ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(0)}));
	// B's whole sword stack leaves B with a plain SM_DELETE_ITEM (:303, ItemDeleteType.DEFAULT)
	EXPECT_EQ(ofOpcode(sentB(), itemtest::SM_DELETE_ITEM_OPCODE), cp::exactly({deleteItem(B_SWORD, DELETE_DEFAULT)}));
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_DELETE_ITEM_OPCODE), cp::exactly({deleteItem(A_POTIONS, DELETE_DEFAULT)}));
}

// :259-260 (D11: InventoryDAO.store for both players on the packet thread): the rows after the trade
TEST_F(ExchangeTest, TheTradeWritesBothInventoriesToTheDatabase) {
	if (!isDatabaseEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";
	setUpDatabaseOnce();
	for (Player* owner : {&a(), &partner()}) {
		for (const runtime::Ptr<Item>& item : owner->getInventory().getItemsWithKinah()) {
			item->setPersistentState(PersistentState::NEW);
			ASSERT_TRUE(dao::InventoryDAO::store(*item, owner->getObjectId()));
		}
	}
	start();
	offerMixed();

	confirm(a());
	confirm(partner());

	expectMixedTradeDone();
	auto column = [](const char* column, int32_t objId) {
		return queryLong("SELECT " + std::string(column) + " FROM inventory WHERE item_unique_id = " + std::to_string(objId));
	};
	EXPECT_EQ(column("item_owner", A_POTIONS), partner().getObjectId()) << "the moved stack is re-owned";
	EXPECT_EQ(column("item_count", A_POTIONS), 100);
	EXPECT_EQ(column("item_owner", B_SWORD), a().getObjectId());
	EXPECT_EQ(column("item_count", B_POTIONS), 30);
	EXPECT_EQ(column("item_owner", B_POTIONS), partner().getObjectId());
	EXPECT_EQ(column("item_count", A_KINAH), 700);
	EXPECT_EQ(column("item_count", B_KINAH), 800);
	runtime::Ptr<Item> copy;
	for (const runtime::Ptr<Item>& item : a().getInventory().getItemsByItemId(MINOR_LIFE_POTION))
		copy = item;
	ASSERT_TRUE(copy);
	EXPECT_EQ(column("item_owner", copy->getObjectId()), a().getObjectId()) << "the split copy is inserted for its new owner";
	EXPECT_EQ(column("item_count", copy->getObjectId()), 20);
	EXPECT_EQ(queryLong("SELECT SUM(item_count) FROM inventory WHERE item_id = " + std::to_string(MINOR_LIFE_POTION)), 150);
}

// :239-246, :309-327: the partner's cube cannot take the offered items: both are told, both exchanges end, nothing moves
TEST_F(ExchangeTest, ATradeThatDoesNotFitEndsTheExchangeWithoutMovingAnything) {
	start();
	offerMixed();
	for (int32_t objId = 902000; partner().getInventory().getFreeSlots() > 0; objId++)
		give(partner(), objId, TRAINING_SWORD, 1);
	confirm(a());
	clearSent();
	clearSentB();

	confirm(partner()); // B is the active player: validateInventorySize(B, A's exchange) fails

	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(ofOpcode(sentB(), itemtest::SM_SYSTEM_MESSAGE_OPCODE),
		cp::exactly({serializedForB(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_HEAVY_TO_ADD_EXCHANGE_ITEM()),
			serializedForB(SM_SYSTEM_MESSAGE::STR_PARTNER_TOO_HEAVY_TO_EXCHANGE())}))
		<< ":315-316 from validateExchange, then :243 from performTrade (A's cube has room)";
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PARTNER_TOO_HEAVY_TO_EXCHANGE())}));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_TRUE(partner().getInventory().getItemByObjId(B_SWORD));
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 50);
	EXPECT_TRUE(ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE).size() == 1u) << "only B's (2)";
}

// :309-321, validateExchange's second arm: the confirming player has room, his partner has none - the partner is told he cannot take the
// items and the confirming player that his partner is too heavy; performTrade then tells the confirming player the first message too
// (:240-241, validateInventorySize(partner, exchange1) is false). Both exchanges end and nothing moves
TEST_F(ExchangeTest, WhenOnlyThePartnerHasNoRoomBothAreToldAndNothingMoves) {
	start();
	offerMixed(); // A offers one item, B two; A has room for two
	for (int32_t objId = 904000; partner().getInventory().getFreeSlots() > 0; objId++)
		give(partner(), objId, TRAINING_SWORD, 1);
	confirm(partner());
	clearSent();
	clearSentB();

	confirm(a()); // A is the active player

	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(ofOpcode(sentB(), itemtest::SM_SYSTEM_MESSAGE_OPCODE),
		cp::exactly({serializedForB(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_HEAVY_TO_ADD_EXCHANGE_ITEM())}));
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE),
		cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PARTNER_TOO_HEAVY_TO_EXCHANGE()),
			serializedFor(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_HEAVY_TO_ADD_EXCHANGE_ITEM())}));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_TRUE(partner().getInventory().getItemByObjId(B_SWORD));
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 50);
	EXPECT_EQ(kinah(partner()), 500);
}

// :295-303: a split offer grown to the whole stack moves the stack itself; the copy made for the first part is dropped and its id released
TEST_F(ExchangeTest, ASplitOfferGrownToTheWholeStackMovesTheStackAndReleasesTheCopysId) {
	start();
	service().addItem(a(), A_POTIONS, 20);
	const int32_t copyId = exchangeOf(a())->getItems().get(A_POTIONS)->getItem()->getObjectId();
	ASSERT_NE(copyId, A_POTIONS);
	service().addItem(a(), A_POTIONS, 80);
	service().addItem(partner(), B_SWORD, 1);
	confirm(a());
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();

	confirm(partner());

	EXPECT_FALSE(a().isTrading());
	ASSERT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS)) << "the stack itself moved";
	EXPECT_EQ(partner().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 100);
	EXPECT_FALSE(partner().getInventory().getItemByObjId(copyId));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 0);
	EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined + 1) << "the copy's id";
	if constexpr (runtime::CHECKED)
		EXPECT_NE(IDFactory::getInstance().recentlyReleased(copyId), nullptr);
}

// :324-327: free slots >= offered items - as many free slots as items is enough (checked before either side's removal)
TEST_F(ExchangeTest, ExactlyAsManyFreeSlotsAsOfferedItemsIsEnough) {
	start();
	offerMixed(); // A offers one item
	for (int32_t objId = 903000; partner().getInventory().getFreeSlots() > 1; objId++)
		give(partner(), objId, TRAINING_SWORD, 1);
	confirm(a());

	confirm(partner());

	EXPECT_FALSE(partner().isTrading());
	EXPECT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS)) << "the trade went through";
	EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
	EXPECT_EQ(kinah(partner()), 500 + 300);
}

// :248-252, :280-307: the confirming player's item is gone: the audit, both exchanges end
TEST_F(ExchangeTest, AnOfferedItemGoneFromTheConfirmingPlayersCubeFailsTheTrade) {
	start();
	offerMixed();
	confirm(partner());
	a().getInventory().delete_(*a().getInventory().getItemByObjId(A_POTIONS)); // e.g. used up while offered
	network::test::LogCapture capture({AUDIT_LOGGER});

	confirm(a()); // A is the active player: his own removal fails first

	EXPECT_TRUE(capture.contains("tried to trade not existing item")) << capture.dump();
	EXPECT_TRUE(capture.contains("tried to exploit kinah exchange with partner: " + partner().toString())) << capture.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_EQ(kinah(partner()), 500);
	EXPECT_TRUE(partner().getInventory().getItemByObjId(B_SWORD)) << "the partner's removal never ran";
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 50);
}

// the same with the roles swapped: the partner's removal runs first and is not undone (pinned as Java has it; docs/deviations/P5-09b.md
// "M5c stage 1": the whole stack stays out of the cube until a relog reloads its unchanged row, the split part and the kinah are gone)
TEST_F(ExchangeTest, WhenTheOtherPlayersItemIsGoneTheConfirmingPlayersRemovalIsNotUndoneAsInJava) {
	start();
	offerMixed();
	confirm(a());
	a().getInventory().delete_(*a().getInventory().getItemByObjId(A_POTIONS));
	network::test::LogCapture capture({AUDIT_LOGGER});

	confirm(partner()); // B is the active player: B's removal succeeds, A's fails

	EXPECT_TRUE(capture.contains("tried to trade not existing item")) << capture.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().getInventory().getItemByObjId(B_SWORD)) << "removed, never returned";
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 30) << "the 20 split off are gone";
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(countOf(a(), TRAINING_SWORD), 1);
	EXPECT_EQ(kinah(partner()), 500) << "B offered no kinah";
	EXPECT_EQ(kinah(a()), 1000) << "A's removal failed before its kinah";
}

// :182-214: cancel returns the view of the offered items and tells the partner; nothing moved, so nothing moves back
TEST_F(ExchangeTest, CancelShowsTheOfferedItemsAgainAndTellsThePartner) {
	start();
	service().addItem(a(), A_SWORD, 1);
	service().addItem(a(), A_POTIONS, 20);
	service().addKinah(partner(), 100);
	const int32_t copyId = exchangeOf(a())->getItems().get(A_POTIONS)->getItem()->getObjectId();
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
	clearSent();
	clearSentB();

	service().cancelExchange(a());

	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	// :191, :273-274: cleanUpExchanges(true) releases the id of the split copy, which never reached a cube; the whole sword keeps its own
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined + 1);
	if constexpr (runtime::CHECKED) {
		EXPECT_NE(IDFactory::getInstance().recentlyReleased(copyId), nullptr);
		EXPECT_EQ(IDFactory::getInstance().recentlyReleased(A_SWORD), nullptr);
	}
	std::vector<std::vector<uint8_t>> own = sent();
	EXPECT_EQ(ofOpcode(own, itemtest::SM_INVENTORY_ADD_ITEM_OPCODE).size(), 1u) << ":206-207 the whole stack: PLAYER_EXCHANGE_GET_BACK";
	EXPECT_EQ(ofOpcode(own, itemtest::SM_INVENTORY_UPDATE_ITEM_OPCODE),
		cp::exactly({serializedFor(SM_INVENTORY_UPDATE_ITEM(a(), *a().getInventory().getItemByObjId(A_POTIONS),
			ItemPacketService_ItemUpdateType::INC_PLAYER_EXCHANGE_GET_BACK))}))
		<< ":208-209 the split: the real stack";
	EXPECT_EQ(ofOpcode(own, itemtest::SM_CUBE_UPDATE_OPCODE).size(), 1u);
	EXPECT_EQ(ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(1)}));
	EXPECT_TRUE(ofOpcode(sentB(), itemtest::SM_CUBE_UPDATE_OPCODE).empty()) << "B offered no item";
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_TRUE(a().getInventory().getItemByObjId(A_SWORD));
	EXPECT_EQ(kinah(partner()), 500);
}

// PlayerLeaveWorldService.java:76-85 (PlayerLeaveWorldService.cpp:84-108): logout first takes the connection away, then cancels the exchange:
// the partner gets the cancel and his items' view back, the leaving player gets nothing
TEST_F(ExchangeTest, LogoutCancelsTheExchangeForThePartner) {
	start();
	service().addItem(a(), A_SWORD, 1);
	service().addItem(partner(), B_SWORD, 1);
	clearSent();
	clearSentB();

	partner().setClientConnection(nullptr); // PlayerLeaveWorldService.leaveWorld: player.setClientConnection(null)
	service().cancelExchange(partner());    // ... ExchangeService.getInstance().cancelExchange(player)

	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(1)}));
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_INVENTORY_ADD_ITEM_OPCODE).size(), 1u) << "A's sword is shown again";
	EXPECT_TRUE(sentB().empty()) << "semi-offline: PacketSendUtility sends nothing";
	EXPECT_TRUE(a().getInventory().getItemByObjId(A_SWORD));
	EXPECT_TRUE(partner().getInventory().getItemByObjId(B_SWORD));
	clientB->enterWorld(b);
}

// :216-224
TEST_F(ExchangeTest, AConfirmationWithoutAnExchangeOrAnOnlinePlayerDoesNothing) {
	service().confirmExchange(nullptr);
	confirm(a());
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());

	start();
	a().setClientConnection(nullptr);
	confirm(a());
	client->enterWorld(f);
	EXPECT_FALSE(exchangeOf(a())->isConfirmed()) << ":217-218 offline";
	EXPECT_TRUE(sentB().empty());
}

/**
 * The interleaving points of one CM_EXCHANGE_OK (D7): the yield points (runtime/base/YieldPoint.h, the hooks of the kernel's PCT tests) its
 * task passes on the test thread while it holds no lock, before it reads or writes a shared field (the Field sites) or acquires a Monitor -
 * the steps between which another connection thread's work can become visible to it. While a sweep exists, the task it arms (the outer
 * confirmation) counts its points, and at point `runAt` the executor runs what is queued (the inner confirmation), whole. Where the outer task
 * holds a lock the inner one would block until the release, which is the next point counted. Kernel-internal sites (retain and release,
 * epochs, map node tables, futures) are not points: no ExchangeService state is read or written there.
 */
class InterleavingSweep {
public:
	InterleavingSweep(runtime::DeterministicExecutor& executor, int64_t runAt)
		: executor(executor), runAt(runAt), thread(std::this_thread::get_id()) {
		active.store(this, std::memory_order_release);
		runtime::pct::installHooks(&HOOKS);
	}
	~InterleavingSweep() {
		runtime::pct::installHooks(nullptr);
		active.store(nullptr, std::memory_order_release);
	}
	InterleavingSweep(const InterleavingSweep&) = delete;
	InterleavingSweep& operator=(const InterleavingSweep&) = delete;

	/** Makes the running task the counted (outer) one while it exists; a null sweep arms nothing */
	class Arm {
	public:
		explicit Arm(InterleavingSweep* sweep) : sweep(sweep) {
			if (sweep != nullptr)
				sweep->armed = true;
		}
		~Arm() {
			if (sweep != nullptr)
				sweep->armed = false;
		}
		Arm(const Arm&) = delete;
		Arm& operator=(const Arm&) = delete;

	private:
		InterleavingSweep* const sweep;
	};

	/** The points the outer task passed (all of them when the queued tasks did not run at one) */
	int64_t pointsPassed() const { return points; }
	/** The tasks the executor ran at point `runAt` (0: the outer task passed fewer points) */
	size_t tasksRunAtThePoint() const { return ran; }

private:
	static void onYield(const char* site) noexcept {
		InterleavingSweep* sweep = active.load(std::memory_order_acquire);
		if (sweep == nullptr || std::this_thread::get_id() != sweep->thread || !sweep->armed || sweep->nested)
			return;
		const std::string_view name(site);
		if (!name.starts_with("Field") && name != "Monitor::lock" && name != "Monitor::tryLock")
			return;
		const runtime::ThreadContext& context = runtime::ThreadContext::current();
		if (context.heldLockCount.load(std::memory_order_relaxed) != 0 || context.heldLeafCount != 0)
			return;
		if (sweep->points++ != sweep->runAt)
			return;
		sweep->nested = true;
		sweep->ran = sweep->executor.runReady();
		sweep->nested = false;
	}

	static inline std::atomic<InterleavingSweep*> active{nullptr};
	static constexpr runtime::pct::PctHooks HOOKS{.yield = &onYield};

	runtime::DeterministicExecutor& executor;
	const int64_t runAt;
	const std::thread::id thread;
	bool armed = false;
	bool nested = false;
	int64_t points = 0;
	size_t ran = 0;
};

/** The lines of the EXCHANGE_LOG logger (ExchangeService.java:32) while it exists; they do not reach the root sink */
class ExchangeLogLines {
public:
	ExchangeLogLines() : sink(std::make_shared<Sink>()) {
		commons::logging::LoggerFactory::configure("EXCHANGE_LOG", {.level = spdlog::level::info, .sinks = {sink}, .additive = false});
	}
	~ExchangeLogLines() { commons::logging::LoggerFactory::removeConfig("EXCHANGE_LOG"); }
	ExchangeLogLines(const ExchangeLogLines&) = delete;
	ExchangeLogLines& operator=(const ExchangeLogLines&) = delete;

	const std::vector<std::string>& lines() const { return sink->lines; }
	std::vector<std::string> sorted() const {
		std::vector<std::string> lines = sink->lines;
		std::ranges::sort(lines);
		return lines;
	}
	void clear() { sink->lines.clear(); }

private:
	struct Sink : spdlog::sinks::base_sink<spdlog::details::null_mutex> {
		std::vector<std::string> lines;

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override { lines.emplace_back(msg.payload.data(), msg.payload.size()); }
		void flush_() override {}
	};

	std::shared_ptr<Sink> sink;
};

/** The fixture's own objects; every other object of a cube is a split copy numbered by IDFactory */
const std::set<int32_t> FIXTURE_OBJECTS{A_POTIONS, A_SWORD, A_JUICE, A_KINAH, B_POTIONS, B_SWORD, B_KINAH};

/** Per item id, the count over both cubes as ExchangeTest stocks them (kinah included) */
const std::map<int32_t, int64_t> INITIAL_TOTALS{{KINAH, 1500}, {TRAINING_SWORD, 2}, {MINOR_LIFE_POTION, 150}, {FRUIT_JUICE, 12}};

class ExchangeRaceTest : public ExchangeTest {
protected:
	/** T-04's four offer mixes (docs/deviations/P5-09b.md, "M5c stage 1") */
	enum class Mix { WHOLE_STACKS, SPLIT_STACKS_AND_KINAH, SPLIT_AGAINST_WHOLE, WHOLE_AGAINST_SPLIT_AND_KINAH };

	/** The player's CM_EXCHANGE_OK as a task on the fixture's DeterministicExecutor (CM_EXCHANGE_OK.java: confirmExchange(player)) */
	void queueConfirm(Player& player) {
		utils::ThreadPoolManager::getInstance().execute(
			runtime::Pin(&player), [&player] { ExchangeService::getInstance().confirmExchange(runtime::Ptr<Player>(player)); });
	}

	/**
	 * The same for a sweep: `sweep` (if not null) counts the interleaving points of this confirmation. A NullPointerException is counted in
	 * `late`, as the packet thread would log it: a confirmation whose exchange a trade ended between two of its reads (ExchangeService.java:227
	 * and :230 read the exchanges again after :220) throws it in Java too. The sweep checks how many there are (sweepEveryInterleaving).
	 */
	void queueConfirm(Player& player, InterleavingSweep* sweep, int32_t* late) {
		utils::ThreadPoolManager::getInstance().execute(runtime::Pin(&player), [&player, sweep, late] {
			InterleavingSweep::Arm arm(sweep);
			try {
				ExchangeService::getInstance().confirmExchange(runtime::Ptr<Player>(player));
			} catch (const runtime::NullPointerException&) {
				++*late;
			}
		});
	}

	/**
	 * A task that opens the pair's next exchange once neither is trading, after the split-against-whole trade: A offers 30 of the 80 potions he
	 * kept, B 10 of his 50 (two split copies, so a trade of it would change both cubes' stacks and copies)
	 */
	void queueNextExchange() {
		utils::ThreadPoolManager::getInstance().execute(runtime::Pin(&a()), [this] {
			if (service().isPlayerInExchange(a()) || service().isPlayerInExchange(partner()))
				return; // the first exchange is still open: the request would be refused (PlayerRestrictions.canTrade, isTrading)
			service().registerExchange(a(), partner());
			service().addItem(a(), A_POTIONS, 30);
			service().addItem(partner(), B_POTIONS, 10);
		});
	}

	/**
	 * A's CM_EXCHANGE_OK while B's is queued after its `confirm()`: B's task runs from the exchange log line of A's first item (see the file
	 * comment). B's confirmation runs with the exchange log off, since the callback runs inside that logger's sink.
	 */
	void raceConfirmations() {
		exchangeOf(partner())->confirm(); // B's task has run ExchangeService.java:225
		queueConfirm(partner());          // ... and the rest of it waits
		ConfigScope<bool> logExchange(configs::main::LoggingConfig::LOG_PLAYER_EXCHANGE, true);
		LogHook hook("EXCHANGE_LOG", "Player Holder exchanged item", [this] {
			configs::main::LoggingConfig::LOG_PLAYER_EXCHANGE.store(false);
			bTasks = executor->runReady();
			configs::main::LoggingConfig::LOG_PLAYER_EXCHANGE.store(true);
		});
		confirm(a());
		EXPECT_TRUE(hook.fired()) << "A's performTrade reached its first put";
	}

	size_t bTasks = 0;

	void offer(Mix mix) {
		switch (mix) {
		case Mix::WHOLE_STACKS:
			offerWholeStacks();
			break;
		case Mix::SPLIT_STACKS_AND_KINAH:
			offerSplitStacksAndKinah();
			break;
		case Mix::SPLIT_AGAINST_WHOLE:
			offerSplitAgainstWhole();
			break;
		case Mix::WHOLE_AGAINST_SPLIT_AND_KINAH:
			offerWholeAgainstSplitAndKinah();
			break;
		}
	}

	/** A his 100 potions, B his sword */
	void offerWholeStacks() {
		service().addItem(a(), A_POTIONS, 100);
		service().addItem(partner(), B_SWORD, 1);
		clearSent();
		clearSentB();
	}

	/** A 20 of his 100 potions and 300 kinah, B 10 of his 50 potions and 100 kinah */
	void offerSplitStacksAndKinah() {
		service().addItem(a(), A_POTIONS, 20);
		service().addKinah(a(), 300);
		service().addItem(partner(), B_POTIONS, 10);
		service().addKinah(partner(), 100);
		clearSent();
		clearSentB();
	}

	/** A 20 of his 100 potions, B his sword */
	void offerSplitAgainstWhole() {
		service().addItem(a(), A_POTIONS, 20);
		service().addItem(partner(), B_SWORD, 1);
		clearSent();
		clearSentB();
	}

	/** A his 100 potions, B 20 of his 50 potions and 100 kinah */
	void offerWholeAgainstSplitAndKinah() {
		service().addItem(a(), A_POTIONS, 100);
		service().addItem(partner(), B_POTIONS, 20);
		service().addKinah(partner(), 100);
		clearSent();
		clearSentB();
	}

	/** The split copy one player's exchange made of a stack */
	int32_t copyIdOf(Player& player, int32_t stackObjId) { return exchangeOf(player)->getItems().get(stackObjId)->getItem()->getObjectId(); }

	/** The pair's two open exchanges, A's and B's, held by the test so that expectOnlyTheTestHolds can count their references after the trade */
	std::vector<runtime::Ref<Exchange>> holdExchanges() {
		return {runtime::Ref<Exchange>(*exchangeOf(a())), runtime::Ref<Exchange>(*exchangeOf(partner()))};
	}

	/**
	 * After the trade, nothing but the test holds the pair's two exchanges: neither ExchangeService's map nor the D7 key set, which holds a Ref to
	 * the pair's key exchange while its trade runs (a key never given back would keep both exchanges, their players and their offered items alive
	 * for the life of the process). A removed map node keeps its Ref until the Reclaimer frees the node, which it does once no scope can see it,
	 * so the test's scope ends first (DropServiceTest's pattern): a Ptr taken before this call is stale after it.
	 */
	void expectOnlyTheTestHolds(const std::vector<runtime::Ref<Exchange>>& exchanges) {
		scope.reset();
		runtime::Reclaimer::getInstance().drain();
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));
		for (size_t i = 0; i < exchanges.size(); ++i)
			EXPECT_EQ(exchanges[i]->refCount(), 1u) << (i == 0 ? "A's" : "B's") << " exchange is still referenced after the trade";
	}

	/** A cube: the fixture's objects by object id ({item id, count}), the split copies by {item id, count} */
	struct Holdings {
		std::map<int32_t, std::pair<int32_t, int64_t>> objects;
		std::multiset<std::pair<int32_t, int64_t>> copies;
	};

	static Holdings holdingsOf(Player& player) {
		Holdings holdings;
		for (const auto& [objId, idCount] : inventoryOf(player)) {
			if (FIXTURE_OBJECTS.contains(objId))
				holdings.objects[objId] = idCount;
			else
				holdings.copies.insert(idCount);
		}
		return holdings;
	}

	/** What the unraced trade of an offer mix ends with (A confirms, then B): every interleaving must end the same */
	struct Reference {
		Holdings ofA;
		Holdings ofB;
		std::vector<std::string> exchangeLog; // sorted: who trades decides the order of the lines, not their text
	};

	Reference unracedTrade(Mix mix, ExchangeLogLines& exchangeLog) {
		restock();
		start();
		offer(mix);
		exchangeLog.clear();
		confirm(a());
		confirm(partner());
		EXPECT_FALSE(a().isTrading());
		EXPECT_FALSE(partner().isTrading());
		EXPECT_EQ(totals(), INITIAL_TOTALS);
		EXPECT_FALSE(exchangeLog.lines().empty());
		return {holdingsOf(a()), holdingsOf(partner()), exchangeLog.sorted()};
	}

	/** The sweep's variants: which confirmation is the outer task, and whether the inner one ran :225 before the outer started */
	struct Variant {
		bool aOuter;
		bool innerConfirmedFirst;
	};

	static std::string describe(const Variant& variant) {
		return std::string(variant.aOuter ? "B's confirmation at a point of A's" : "A's confirmation at a point of B's") +
			(variant.innerConfirmedFirst ? ", its :225 run before" : "");
	}

	/** What one interleaving did, beyond the checks interleave() makes */
	struct Outcome {
		int64_t points = 0;             // the outer confirmation's interleaving points (all of them when the inner did not run at one)
		bool innerRanAtThePoint = false;
		bool aTraded = false;           // A's performTrade traded (the giver of the first exchange log line, :336-344)
		bool innerCheckedInTheTrade = false; // the inner confirmation passed :230 while the outer's trade ran (T-04's window)
		int32_t lateOuter = 0;          // confirmations that found their exchange ended (NullPointerException, as in Java): the outer one
		int32_t lateInner = 0;          // ... the inner one
		bool nextExchangeOpen = false;
	};

	/**
	 * One interleaving: the offer mix, the inner confirmation at point `runAt` of the outer one, and the checks that the trade ran exactly once:
	 * both cubes as after the unraced trade, each put logged once, the no-duplication invariant, no audit line, no object id released, one
	 * SM_EXCHANGE_CONFIRMATION(0) to each player, and the pair's key given back. With `nextExchange`, a task queued after the inner confirmation
	 * opens the pair's next exchange as soon as the first one has ended; nothing of it may move.
	 */
	Outcome interleave(Mix mix, const Variant& variant, int64_t runAt, const Reference& reference, ExchangeLogLines& exchangeLog,
		const network::test::LogCapture& audit, bool nextExchange) {
		restock();
		start();
		offer(mix);
		Player& outer = variant.aOuter ? a() : partner();
		Player& inner = variant.aOuter ? partner() : a();
		if (variant.innerConfirmedFirst)
			exchangeOf(inner)->confirm();
		const std::vector<runtime::Ref<Exchange>> held = holdExchanges();
		const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
		exchangeLog.clear();
		Outcome outcome;
		{
			InterleavingSweep sweep(*executor, runAt);
			queueConfirm(outer, &sweep, &outcome.lateOuter);
			queueConfirm(inner, nullptr, &outcome.lateInner);
			if (nextExchange)
				queueNextExchange();
			executor->runReady();
			outcome.points = sweep.pointsPassed();
			outcome.innerRanAtThePoint = sweep.tasksRunAtThePoint() > 0;
		}

		const Holdings ofA = holdingsOf(a());
		const Holdings ofB = holdingsOf(partner());
		EXPECT_EQ(ofA.objects, reference.ofA.objects);
		EXPECT_EQ(ofA.copies, reference.ofA.copies);
		EXPECT_EQ(ofB.objects, reference.ofB.objects);
		EXPECT_EQ(ofB.copies, reference.ofB.copies);
		EXPECT_EQ(exchangeLog.sorted(), reference.exchangeLog) << "every put of the trade (:336-344) once";
		EXPECT_EQ(totals(), INITIAL_TOTALS) << "the no-duplication invariant";
		expectNoSharedObject();
		EXPECT_EQ(audit.count("tried to"), 0) << audit.dump();
		EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined) << "no object id released";
		if constexpr (runtime::CHECKED) {
			for (Player* player : {&a(), &partner()})
				for (const auto& [objId, idCount] : inventoryOf(*player))
					EXPECT_EQ(IDFactory::getInstance().recentlyReleased(objId), nullptr) << "the live object " << objId << " has its id released";
		}
		const std::vector<std::vector<uint8_t>> toA = ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE);
		const std::vector<std::vector<uint8_t>> toB = ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE);
		EXPECT_EQ(std::ranges::count(toA, exchangeConfirmation(0)), 1) << "one trade (:254-255)";
		EXPECT_EQ(std::ranges::count(toB, exchangeConfirmation(0)), 1);
		EXPECT_LE(std::ranges::count(toA, exchangeConfirmation(2)), 1) << "one confirmation of B (:228)";
		EXPECT_LE(std::ranges::count(toB, exchangeConfirmation(2)), 1);
		EXPECT_EQ(std::ranges::count(toA, exchangeConfirmation(1)) + std::ranges::count(toB, exchangeConfirmation(1)), 0) << "no cancel";

		if (nextExchange) {
			outcome.nextExchangeOpen = service().isPlayerInExchange(a()); // it waits, and nothing of it moved (the cubes above)
		} else {
			EXPECT_FALSE(a().isTrading());
			EXPECT_FALSE(partner().isTrading());
		}

		outcome.aTraded = !exchangeLog.lines().empty() && exchangeLog.lines().front().starts_with("Player Holder ");
		// the inner confirmation's (2) reached the outer player after the (0) of the outer's trade: it passed :230 during that trade (its
		// exchange is still registered until :262, and a confirmation without one returns at :223 before sending anything)
		const std::vector<std::vector<uint8_t>>& toOuter = variant.aOuter ? toA : toB;
		const auto zero = std::ranges::find(toOuter, exchangeConfirmation(0));
		const auto two = std::ranges::find(toOuter, exchangeConfirmation(2));
		outcome.innerCheckedInTheTrade =
			outcome.innerRanAtThePoint && outcome.aTraded == variant.aOuter && zero != toOuter.end() && two != toOuter.end() && two > zero;
		expectOnlyTheTestHolds(held);
		return outcome;
	}

	/**
	 * The sweep of an offer mix (one context switch, not every schedule): for each variant, the inner confirmation, whole, at every interleaving
	 * point of the outer one, and once after it (the first point the outer did not reach). Stops at the first interleaving that fails.
	 */
	void sweepEveryInterleaving(Mix mix, bool nextExchange = false) {
#if !AION_PCT
		static_cast<void>(mix);
		static_cast<void>(nextExchange);
		GTEST_SKIP() << "the interleaving points are the PCT yield points, compiled only into checked builds (AION_PCT)";
#else
		ConfigScope<bool> logExchange(configs::main::LoggingConfig::LOG_PLAYER_EXCHANGE, true);
		ExchangeLogLines exchangeLog;
		network::test::LogCapture audit({AUDIT_LOGGER});
		const Reference reference = unracedTrade(mix, exchangeLog);
		ASSERT_FALSE(HasFailure());
		for (const Variant variant : {Variant{true, false}, Variant{true, true}, Variant{false, false}, Variant{false, true}}) {
			if (nextExchange && !variant.innerConfirmedFirst)
				continue; // a lost claim needs both confirmations past :230
			int64_t points = -1;
			int32_t aTraded = 0;
			int32_t bTraded = 0;
			int32_t inTheTrade = 0;
			int32_t lateOuter = 0;
			int32_t lateInner = 0;
			int32_t nextOpen = 0;
			for (int64_t runAt = 0; points < 0; ++runAt) {
				SCOPED_TRACE(describe(variant) + ", point " + std::to_string(runAt));
				const Outcome outcome = interleave(mix, variant, runAt, reference, exchangeLog, audit, nextExchange);
				if (HasFailure())
					return;
				(outcome.aTraded ? aTraded : bTraded)++;
				inTheTrade += outcome.innerCheckedInTheTrade ? 1 : 0;
				lateOuter += outcome.lateOuter;
				lateInner += outcome.lateInner;
				nextOpen += outcome.nextExchangeOpen ? 1 : 0;
				if (!outcome.innerRanAtThePoint)
					points = outcome.points;
			}
			std::cout << "[ D7 sweep ] " << describe(variant) << ": " << points << " points; A traded in " << aTraded << ", B in " << bTraded
					  << "; the inner confirmation passed :230 during the trade in " << inTheTrade << "; late confirmations " << lateOuter
					  << " outer, " << lateInner << " inner"
					  << (nextExchange ? "; next exchange opened in " + std::to_string(nextOpen) : std::string()) << "\n";
			// Java's NullPointerExceptions (:227-230, before the key) and no other; one more is a confirmation that took the key and threw after
			// it, e.g. in performTrade on a pair its partner's trade has ended. The outer confirmation throws when the inner one trades whole at
			// one of the 4 points between the outer's confirm() (:225) and its read of the partner's exchange (:230) - unless the next exchange,
			// opened at once, gives the outer player an exchange again. (At the next point, after that read, the outer one reads the ended
			// exchange as confirmed, takes the free key and returns at the re-check.) With :225 run first the inner one throws once, run between
			// the two removals of the outer's clean-up (:262, :270 for each player in turn): its own exchange still registered, its partner's
			// gone.
			EXPECT_EQ(lateOuter, nextExchange ? 0 : 4) << describe(variant) << ": late outer confirmations";
			EXPECT_EQ(lateInner, variant.innerConfirmedFirst ? 1 : 0) << describe(variant) << ": late inner confirmations";
			EXPECT_GT(points, 0) << describe(variant);
			EXPECT_GT(aTraded, 0) << describe(variant) << ": some interleaving lets A's confirmation trade";
			EXPECT_GT(bTraded, 0) << describe(variant) << ": some interleaving lets B's confirmation trade";
			if (variant.innerConfirmedFirst)
				EXPECT_GT(inTheTrade, 0) << describe(variant) << ": T-04's window, where Java trades twice, was reached";
			if (nextExchange)
				EXPECT_GT(nextOpen, 0) << describe(variant) << ": the next exchange opened while a confirmation of the first one ran";
		}
#endif
	}
};

// the two orders a single executor can give: whichever confirmation comes second trades, once
TEST_F(ExchangeRaceTest, QueuedConfirmationsTradeOnceWhenAConfirmsFirst) {
	start();
	offerMixed();
	queueConfirm(a());
	queueConfirm(partner());

	executor->runReady();

	expectMixedTradeDone();
	EXPECT_EQ(ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(2), exchangeConfirmation(0)}));
	EXPECT_EQ(ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(2), exchangeConfirmation(0)}));
}

TEST_F(ExchangeRaceTest, QueuedConfirmationsTradeOnceWhenBConfirmsFirst) {
	start();
	offerMixed();
	queueConfirm(partner());
	queueConfirm(a());

	executor->runReady();

	expectMixedTradeDone();
}

// D7: the confirmation that trades takes the pair's key and gives it back when performTrade has returned, in both unraced orders (each order
// has the other player's confirmation take the key, so both arms of the key's choice by object id run); the sweeps check it in every
// interleaving, among them the confirmation that takes the key and returns at the re-check
TEST_F(ExchangeRaceTest, TheTradeGivesThePairsKeyBack) {
	for (const bool aFirst : {true, false}) {
		SCOPED_TRACE(aFirst ? "A confirms first, B's confirmation trades" : "B confirms first, A's confirmation trades");
		restock();
		start();
		offerMixed();
		const std::vector<runtime::Ref<Exchange>> held = holdExchanges();

		confirm(aFirst ? a() : partner());
		confirm(aFirst ? partner() : a());

		expectMixedTradeDone();
		expectOnlyTheTestHolds(held);
	}
}

// D7 fixed, T-04's interleaving, whole stacks. Java: B's performTrade finds its sword gone, audits "tried to trade not existing item" and "tried
// to exploit kinah exchange" and ends both exchanges. Now B's confirmation finds the pair's trade started and returns after its (2)
TEST_F(ExchangeRaceTest, TheMeasuredInterleavingTradesWholeStacksOnceWithoutAnAudit) {
	start();
	offerWholeStacks();
	network::test::LogCapture audit({AUDIT_LOGGER});

	raceConfirmations();

	EXPECT_GE(bTasks, 1u) << "B's CM_EXCHANGE_OK ran inside A's trade";
	EXPECT_FALSE(audit.contains("tried to")) << audit.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
	EXPECT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS));
	EXPECT_EQ(totals(), INITIAL_TOTALS);
	expectNoSharedObject();
	// B's (2) reached A from inside A's trade, after A's (0); the trade's (0) went to each player once
	EXPECT_EQ(ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(0), exchangeConfirmation(2)}));
	EXPECT_EQ(ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(2), exchangeConfirmation(0)}));
}

// D7 fixed, T-04's interleaving, split stacks and kinah. Java: B's performTrade succeeds too - both removals run twice, 30 potions are destroyed
// and the kinah moves twice each way (A paid 600 and got 200). Now the trade runs once
TEST_F(ExchangeRaceTest, TheMeasuredInterleavingTradesSplitStacksAndKinahOnce) {
	start();
	offerSplitStacksAndKinah();

	raceConfirmations();

	EXPECT_GE(bTasks, 1u);
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(a().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 100 - 20);
	EXPECT_EQ(partner().getInventory().getItemByObjId(B_POTIONS)->getItemCount(), 50 - 10);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 80 + 10) << "B's copy arrived";
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 40 + 20) << "A's copy arrived";
	EXPECT_EQ(kinah(a()), 1000 - 300 + 100);
	EXPECT_EQ(kinah(partner()), 500 - 100 + 300);
	EXPECT_EQ(totals(), INITIAL_TOTALS) << "nothing destroyed, nothing duplicated";
	expectNoSharedObject();
	EXPECT_EQ(std::ranges::count(ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE), exchangeConfirmation(0)), 1) << "one trade";
	EXPECT_EQ(std::ranges::count(ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE), exchangeConfirmation(0)), 1);
}

// D7 fixed, T-04's interleaving, A's split stack against B's whole stack. Java: B's performTrade fails on its sword and its
// cleanUpExchanges(true, B, A) (:249, :273-274) releases the id of A's split copy, already live in B's cube. Now nothing is released
TEST_F(ExchangeRaceTest, TheMeasuredInterleavingKeepsTheIdOfASplitCopyAlreadyPut) {
	start();
	offerSplitAgainstWhole();
	const int32_t copyId = copyIdOf(a(), A_POTIONS);
	ASSERT_NE(copyId, A_POTIONS);
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
	network::test::LogCapture audit({AUDIT_LOGGER});

	raceConfirmations();

	EXPECT_GE(bTasks, 1u);
	EXPECT_FALSE(audit.contains("tried to")) << audit.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	ASSERT_TRUE(partner().getInventory().getItemByObjId(copyId)) << "the copy lives in B's cube";
	EXPECT_EQ(partner().getInventory().getItemByObjId(copyId)->getItemCount(), 20);
	EXPECT_EQ(a().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 80);
	EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
	EXPECT_EQ(totals(), INITIAL_TOTALS);
	expectNoSharedObject();
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined) << "no id released";
	if constexpr (runtime::CHECKED)
		EXPECT_EQ(IDFactory::getInstance().recentlyReleased(copyId), nullptr);
}

// D7 fixed, T-04's interleaving, A's whole stack against B's split stack and kinah. Java: B's performTrade takes B's split part and kinah a
// second time, fails on A's potions and releases the id of B's copy before A receives it (20 potions and 100 kinah destroyed, a live item with
// a released id). Now B keeps them, A gets the copy once and its id stays taken
TEST_F(ExchangeRaceTest, TheMeasuredInterleavingKeepsTheSplitCopyAndKinahOfTheWholeStacksPartner) {
	start();
	offerWholeAgainstSplitAndKinah();
	const int32_t copyId = copyIdOf(partner(), B_POTIONS);
	ASSERT_NE(copyId, B_POTIONS);
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
	network::test::LogCapture audit({AUDIT_LOGGER});

	raceConfirmations();

	EXPECT_GE(bTasks, 1u);
	EXPECT_FALSE(audit.contains("tried to")) << audit.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(partner().getInventory().getItemByObjId(B_POTIONS)->getItemCount(), 50 - 20) << "B's split part taken once";
	EXPECT_EQ(kinah(partner()), 500 - 100) << "and B's kinah";
	EXPECT_EQ(kinah(a()), 1000 + 100);
	ASSERT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS));
	EXPECT_EQ(partner().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 100);
	ASSERT_TRUE(a().getInventory().getItemByObjId(copyId)) << "the copy lives in A's cube";
	EXPECT_EQ(a().getInventory().getItemByObjId(copyId)->getItemCount(), 20);
	EXPECT_EQ(totals(), INITIAL_TOTALS) << "nothing destroyed";
	expectNoSharedObject();
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined) << "no id released";
	if constexpr (runtime::CHECKED)
		EXPECT_EQ(IDFactory::getInstance().recentlyReleased(copyId), nullptr);
}

// D7 fixed, the sweeps of T-04's offer mixes (one confirmation, whole, at every interleaving point of the other: see sweepEveryInterleaving):
// the trade runs exactly once and ends as the unraced one
TEST_F(ExchangeRaceTest, EveryInterleavingTradesWholeStacksOnce) {
	sweepEveryInterleaving(Mix::WHOLE_STACKS);
}

TEST_F(ExchangeRaceTest, EveryInterleavingTradesSplitStacksAndKinahOnce) {
	sweepEveryInterleaving(Mix::SPLIT_STACKS_AND_KINAH);
}

TEST_F(ExchangeRaceTest, EveryInterleavingTradesASplitAgainstAWholeStackOnce) {
	sweepEveryInterleaving(Mix::SPLIT_AGAINST_WHOLE);
}

TEST_F(ExchangeRaceTest, EveryInterleavingTradesAWholeStackAgainstASplitAndKinahOnce) {
	sweepEveryInterleaving(Mix::WHOLE_AGAINST_SPLIT_AND_KINAH);
}

// D7 fixed: a confirmation that passed :230 but lost the pair to its partner's, which traded and ended the exchange before this one took the
// pair's key, trades nothing - neither the ended exchange nor the pair's next one, opened meanwhile, whose offers nobody confirmed
TEST_F(ExchangeRaceTest, AConfirmationThatLostThePairNeverTradesItsNextExchange) {
	sweepEveryInterleaving(Mix::SPLIT_AGAINST_WHOLE, true);
}

} // namespace
} // namespace aion::gameserver::economy::test::trade
