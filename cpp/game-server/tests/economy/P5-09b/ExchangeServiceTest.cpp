// M5c T-02 / T-04 (m5c-plan.md §5, P5-09b): ExchangeService's state machine between two players - registering, adding kinah and items (the
// whole stack, the split copy, the refusals, the staff restriction of AdminService, the temporary trade time of W-07), locking, the two
// confirmations and the trade with its two inventory writes (D11), the refused validation of either side, an item gone before the trade,
// cancel and logout, the no-duplication invariant, the object ids (IDFactory) of the split copies, and the double-confirm race of D7 on the
// fixture's DeterministicExecutor.
//
// Java: ExchangeService.java:43-346. A completed trade calls InventoryDAO.store for both players (:259-260); the DAO logs and swallows an
// SQLException, so without the test database only the case that reads the rows back is skipped.
//
// D7 (m5c-plan.md §4): each player's CM_EXCHANGE_OK is a task; `confirm()` (:225) and the partner's `isConfirmed()` (:230) are a
// check-then-act on plain fields, so two tasks running at once can both pass the check. The race cases queue B's CM_EXCHANGE_OK on the
// executor after its first statement (the test runs `Exchange.confirm()` of B's exchange, which is all :225 does), run A's CM_EXCHANGE_OK,
// and let the executor run B's task at a named point inside A's performTrade: the exchange log line Java writes right after it put A's first
// item into B's cube (:336-338, LoggingConfig.LOG_PLAYER_EXCHANGE on). That is the interleaving "both players passed :230; B's performTrade runs
// after A removed both players' items (:248) and before A stored and cleaned up (:259-262)".

#include "TradeTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

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
#include "aion/gameserver/runtime/collections/Rc.h"
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
		give(a(), A_POTIONS, MINOR_LIFE_POTION, 100);
		give(a(), A_SWORD, TRAINING_SWORD, 1);
		give(a(), A_JUICE, FRUIT_JUICE, 12);
		setKinah(a(), A_KINAH, 1000);
		give(partner(), B_POTIONS, MINOR_LIFE_POTION, 50);
		give(partner(), B_SWORD, TRAINING_SWORD, 1);
		setKinah(partner(), B_KINAH, 500);
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

class ExchangeRaceTest : public ExchangeTest {
protected:
	/** The player's CM_EXCHANGE_OK as a task on the fixture's DeterministicExecutor (CM_EXCHANGE_OK.java: confirmExchange(player)) */
	void queueConfirm(Player& player) {
		utils::ThreadPoolManager::getInstance().execute(
			runtime::Pin(&player), [&player] { ExchangeService::getInstance().confirmExchange(runtime::Ptr<Player>(player)); });
	}

	/**
	 * A's CM_EXCHANGE_OK while B's is queued after its `confirm()`: B's task runs from the exchange log line of A's first item (see the file
	 * comment). B's trade runs with the exchange log off, since the callback runs inside that logger's sink.
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

// D7, whole stacks: B's performTrade finds its sword already gone from its cube, audits and ends both exchanges; A's trade completes. The
// trade happens once and every item id is conserved - the consequence D7 expected ("a failed second removeItemsFromInventory and an audit
// line"), measured
TEST_F(ExchangeRaceTest, BothPassTheCheckAndTheSecondTradeFailsOnTheRemovedWholeStacks) {
	start();
	service().addItem(a(), A_POTIONS, 100);
	service().addItem(partner(), B_SWORD, 1);
	network::test::LogCapture audit({AUDIT_LOGGER});

	raceConfirmations();

	EXPECT_GE(bTasks, 1u) << "B's CM_EXCHANGE_OK ran inside A's trade";
	EXPECT_TRUE(audit.contains(partner().toString() + " tried to trade not existing item")) << audit.dump();
	EXPECT_TRUE(audit.contains(partner().toString() + " tried to exploit kinah exchange with partner: " + a().toString())) << audit.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
	EXPECT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS));
	EXPECT_EQ(totals(), (std::map<int32_t, int64_t>{{KINAH, 1500}, {TRAINING_SWORD, 2}, {MINOR_LIFE_POTION, 150}, {FRUIT_JUICE, 12}}));
	expectNoSharedObject();
	// B's (2) reached A from inside A's trade, after A's (0)
	EXPECT_EQ(ofOpcode(sent(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(0), exchangeConfirmation(2)}));
}

// D7, split stacks and kinah: B's performTrade succeeds too - both removals run twice, each split copy is put once (the second put of the same
// object is refused, ItemStorage.putItem's putIfAbsent) and the kinah moves twice each way. Nothing is duplicated, but 30 potions are destroyed
// (A's second 20, B's second 10) and the kinah trade is doubled (A pays 600 and gets 200). Measured, pinned as Java has it; the fix is the
// user's (D7, docs/deviations/P5-09b.md "M5c stage 1")
TEST_F(ExchangeRaceTest, BothPassTheCheckAndSplitStacksAreRemovedTwiceAsInJava) {
	start();
	service().addItem(a(), A_POTIONS, 20);
	service().addKinah(a(), 300);
	service().addItem(partner(), B_POTIONS, 10);
	service().addKinah(partner(), 100);

	raceConfirmations();

	EXPECT_GE(bTasks, 1u);
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(a().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 100 - 20 - 20);
	EXPECT_EQ(partner().getInventory().getItemByObjId(B_POTIONS)->getItemCount(), 50 - 10 - 10);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 60 + 10) << "B's copy arrived once";
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 30 + 20) << "A's copy arrived once";
	EXPECT_EQ(kinah(a()), 1000 - 300 - 300 + 100 + 100);
	EXPECT_EQ(kinah(partner()), 500 - 100 - 100 + 300 + 300);
	std::map<int32_t, int64_t> after = totals();
	EXPECT_EQ(after[KINAH], 1500) << "kinah is conserved, moved twice";
	EXPECT_EQ(after[MINOR_LIFE_POTION], 150 - 30) << "the second removals destroy 30 potions";
	expectNoSharedObject();
}

// D7, A's split stack against B's whole stack: B's performTrade fails on its sword (already out of B's cube) and ends both exchanges with
// cleanUpExchanges(true, B, A) (:249-250). For A's exchange that looks for A's split copy in A's cube (:273-274); the copy is not there, since
// A's performTrade has just put it into B's cube, so its id is released while the copy lives on. Every item is conserved, but the id of a live
// item is free: Java's IDFactory.releaseId moves nextMinId down to it (IDFactory.java:178-184), so the next nextId() hands it out again; the
// port's 300 s quarantine and monotone cursor only delay that (IDFactory.h). Measured, pinned as Java has it (D7, docs/deviations/P5-09b.md)
TEST_F(ExchangeRaceTest, BothPassTheCheckAndASplitCopyAlreadyPutHasItsIdReleasedAsInJava) {
	start();
	service().addItem(a(), A_POTIONS, 20);
	service().addItem(partner(), B_SWORD, 1);
	const int32_t copyId = exchangeOf(a())->getItems().get(A_POTIONS)->getItem()->getObjectId();
	ASSERT_NE(copyId, A_POTIONS);
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
	network::test::LogCapture audit({AUDIT_LOGGER});

	raceConfirmations();

	EXPECT_GE(bTasks, 1u);
	EXPECT_TRUE(audit.contains(partner().toString() + " tried to trade not existing item")) << audit.dump();
	EXPECT_TRUE(audit.contains(partner().toString() + " tried to exploit kinah exchange with partner: " + a().toString())) << audit.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	ASSERT_TRUE(partner().getInventory().getItemByObjId(copyId)) << "the copy lives in B's cube";
	EXPECT_EQ(partner().getInventory().getItemByObjId(copyId)->getItemCount(), 20);
	EXPECT_EQ(a().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 80);
	EXPECT_TRUE(a().getInventory().getItemByObjId(B_SWORD));
	EXPECT_EQ(totals(), (std::map<int32_t, int64_t>{{KINAH, 1500}, {TRAINING_SWORD, 2}, {MINOR_LIFE_POTION, 150}, {FRUIT_JUICE, 12}}));
	expectNoSharedObject();
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined + 1) << "the live copy's id was released";
	if constexpr (runtime::CHECKED)
		EXPECT_NE(IDFactory::getInstance().recentlyReleased(copyId), nullptr);
}

// D7, A's whole stack against B's split stack and kinah: B's performTrade takes B's split part and B's kinah a second time, then fails on A's
// potions (already out of A's cube), and cleanUpExchanges(true, B, A) looks for B's split copy in B's cube: the copy is not there (it waits
// in B's exchange to be put into A's cube), so its id is released. A's performTrade then puts that copy into A's cube and pays B's kinah
// once. B loses 20 potions and 100 kinah that nobody receives, and A holds a live item whose id is free. Measured, pinned as Java has it
// (D7, docs/deviations/P5-09b.md)
TEST_F(ExchangeRaceTest, BothPassTheCheckAndTheWholeStackSideReleasesTheSplitCopyItIsAboutToGetAsInJava) {
	start();
	service().addItem(a(), A_POTIONS, 100);
	service().addItem(partner(), B_POTIONS, 20);
	service().addKinah(partner(), 100);
	const int32_t copyId = exchangeOf(partner())->getItems().get(B_POTIONS)->getItem()->getObjectId();
	ASSERT_NE(copyId, B_POTIONS);
	const int32_t quarantined = IDFactory::getInstance().getQuarantinedCount();
	network::test::LogCapture audit({AUDIT_LOGGER});

	raceConfirmations();

	EXPECT_GE(bTasks, 1u);
	EXPECT_TRUE(audit.contains(a().toString() + " tried to trade not existing item")) << audit.dump();
	EXPECT_TRUE(audit.contains(partner().toString() + " tried to exploit kinah exchange with partner: " + a().toString())) << audit.dump();
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(partner().getInventory().getItemByObjId(B_POTIONS)->getItemCount(), 50 - 20 - 20) << "B's split part taken twice";
	EXPECT_EQ(kinah(partner()), 500 - 100 - 100) << "and B's kinah";
	EXPECT_EQ(kinah(a()), 1000 + 100) << "paid once";
	ASSERT_TRUE(partner().getInventory().getItemByObjId(A_POTIONS));
	EXPECT_EQ(partner().getInventory().getItemByObjId(A_POTIONS)->getItemCount(), 100);
	ASSERT_TRUE(a().getInventory().getItemByObjId(copyId)) << "the copy lives in A's cube";
	EXPECT_EQ(a().getInventory().getItemByObjId(copyId)->getItemCount(), 20);
	std::map<int32_t, int64_t> after = totals();
	EXPECT_EQ(after[MINOR_LIFE_POTION], 150 - 20) << "20 potions destroyed";
	EXPECT_EQ(after[KINAH], 1500 - 100) << "100 kinah destroyed";
	expectNoSharedObject();
	EXPECT_EQ(IDFactory::getInstance().getQuarantinedCount(), quarantined + 1) << "the live copy's id was released";
	if constexpr (runtime::CHECKED)
		EXPECT_NE(IDFactory::getInstance().recentlyReleased(copyId), nullptr);
}

} // namespace
} // namespace aion::gameserver::economy::test::trade
