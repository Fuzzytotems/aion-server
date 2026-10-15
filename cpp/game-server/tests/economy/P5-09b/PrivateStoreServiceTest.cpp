// M5c T-03 / T-04 (m5c-plan.md §5, P5-09b): PrivateStoreService - opening a store (its nine refusals and the item validation), the store name,
// selling to a buyer with the store's index semantics, the kinah ledger, the refusals of a sale, closing when sold out, and a socketed item
// (W-25: ItemService.copyItemInfo -> ItemSocketService.addManaStone, ported in stage 0).
//
// Java: PrivateStoreService.java:35-231. Expectations are the Java arithmetic of each body and the rows of TradeTestSupport.h; messages are
// compared with the server's own serialization of the SM_SYSTEM_MESSAGE Java builds there (its bytes are pinned by tests/sm_lz).

#include "TradeTestSupport.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE_NAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

namespace aion::gameserver::economy::test::trade {
namespace {

using model::gameobjects::state::CreatureState;
using model::trade::TradePSItem;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::PrivateStoreService;

const char* EXCHANGE_LOGGER = "EXCHANGE_LOG";
const char* AUDIT_LOGGER = "AUDIT_LOG";

constexpr int32_t SM_EMOTION_OPCODE = itemtest::SM_EMOTION_OPCODE;
constexpr int32_t OPEN_PRIVATESHOP = 33;  // EmotionType.OPEN_PRIVATESHOP(33) (EmotionType.java)
constexpr int32_t CLOSE_PRIVATESHOP = 34; // EmotionType.CLOSE_PRIVATESHOP(34)

// A's items
constexpr int32_t POTIONS = 800001;     // 100 x Minor Life Potion
constexpr int32_t SWORD = 800002;       // 1 x Training Sword
constexpr int32_t JUICE = 800003;       // 12 x Mercenary's Fruit Juice (not tradeable)
constexpr int32_t MORE_POTIONS = 800004; // a second potion stack
constexpr int32_t A_KINAH = 800009;
constexpr int32_t B_KINAH = 800109;

/** The store rows of a request (CM_PRIVATE_STORE builds one TradePSItem per row, CM_PRIVATE_STORE.java:31-36) */
std::vector<runtime::Ref<TradePSItem>> rows(std::initializer_list<std::array<int64_t, 4>> values) {
	std::vector<runtime::Ref<TradePSItem>> result;
	for (const std::array<int64_t, 4>& v : values)
		result.push_back(TradePSItem::create(static_cast<int32_t>(v[0]), static_cast<int32_t>(v[1]), v[2], v[3]));
	return result;
}

std::vector<runtime::Ptr<TradePSItem>> span(const std::vector<runtime::Ref<TradePSItem>>& refs) {
	return std::vector<runtime::Ptr<TradePSItem>>(refs.begin(), refs.end());
}

/** The emotion type byte of a captured SM_EMOTION (SM_EMOTION.java writeImpl: D sender, C type) */
int32_t emotionOf(const std::vector<uint8_t>& packet, int32_t& sender) {
	PacketReader reader(cp::bodyOf(packet));
	sender = reader.D();
	return reader.C();
}

class PrivateStoreTest : public TradeTest {
protected:
	void SetUp() override {
		TradeTest::SetUp();
		give(a(), POTIONS, MINOR_LIFE_POTION, 100);
		give(a(), SWORD, TRAINING_SWORD, 1);
		give(a(), JUICE, FRUIT_JUICE, 12);
		setKinah(a(), A_KINAH, 1000);
		setKinah(partner(), B_KINAH, 10000);
	}

	void open(const std::vector<runtime::Ref<TradePSItem>>& request) {
		std::vector<runtime::Ptr<TradePSItem>> ptrs = span(request);
		PrivateStoreService::createStoreWithItems(a(), ptrs);
	}

	/** A buys through B's CM_BUY_ITEM(seller A, 0, [index, count]...) (CM_BUY_ITEM.java:101-105: the "item id" is the store index) */
	void buy(std::initializer_list<std::pair<int32_t, int64_t>> indexCounts) {
		runtime::Ref<model::trade::TradeList> tradeList = model::trade::TradeList::create(a().getObjectId());
		for (const auto& [index, count] : indexCounts)
			tradeList->addItem(index, count);
		PrivateStoreService::sellStoreItem(a(), partner(), *tradeList);
	}

	/** The store's rows in insertion order: {object id, count, price} */
	std::vector<std::array<int64_t, 3>> storeRows() {
		std::vector<std::array<int64_t, 3>> result;
		for (const runtime::Ptr<TradePSItem>& item : a().getStore()->getSoldItems()->values())
			result.push_back({item->getItemObjId(), item->getCount(), item->getPrice()});
		return result;
	}

	/** A refusal of canOpenPrivateStore: no store, and exactly `expected` sent (PrivateStoreService.java:52-83) */
	void expectRefused(const std::vector<std::vector<uint8_t>>& expected) {
		open(rows({{POTIONS, MINOR_LIFE_POTION, 10, 100}}));
		EXPECT_FALSE(a().getStore());
		EXPECT_EQ(sent(), expected);
		EXPECT_TRUE(ofOpcode(sentB(), SM_EMOTION_OPCODE).empty());
	}
};

// PrivateStoreService.java:35-50: the validated rows in request order, the state replaced by PRIVATE_SHOP, the shop emotion to A and to the
// players who know him
TEST_F(PrivateStoreTest, OpeningRegistersTheRowsInOrderReplacesTheStateAndBroadcastsTheShop) {
	a().setState(CreatureState::RESTING, true); // a seated player: PRIVATE_SHOP (1+2+8, exact) must replace the state, not be or-ed into it

	open(rows({{SWORD, TRAINING_SWORD, 1, 5000}, {POTIONS, MINOR_LIFE_POTION, 30, 100}}));

	ASSERT_TRUE(a().getStore());
	EXPECT_EQ(storeRows(), (std::vector<std::array<int64_t, 3>>{{SWORD, 1, 5000}, {POTIONS, 30, 100}})) << "LinkedHashMap: insertion order";
	EXPECT_EQ(a().getState(), model::gameobjects::state::getId(CreatureState::PRIVATE_SHOP)) << "setState(PRIVATE_SHOP, true) replaces";
	EXPECT_TRUE(a().isInState(CreatureState::PRIVATE_SHOP));
	std::vector<std::vector<uint8_t>> own = ofOpcode(sent(), SM_EMOTION_OPCODE);
	ASSERT_EQ(own.size(), 1u);
	int32_t sender = 0;
	EXPECT_EQ(emotionOf(own[0], sender), OPEN_PRIVATESHOP);
	EXPECT_EQ(sender, a().getObjectId());
	std::vector<std::vector<uint8_t>> seen = ofOpcode(sentB(), SM_EMOTION_OPCODE);
	ASSERT_EQ(seen.size(), 1u) << "broadcastPacket(player, ..., true) reaches the known players";
	EXPECT_EQ(emotionOf(seen[0], sender), OPEN_PRIVATESHOP);
	EXPECT_EQ(sender, a().getObjectId());
}

// :115-122
TEST_F(PrivateStoreTest, ClosingDropsTheStoreRestoresActiveAndBroadcasts) {
	open(rows({{POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	ASSERT_TRUE(a().getStore());
	clearSent();
	clearSentB();

	PrivateStoreService::closePrivateStore(a());

	EXPECT_FALSE(a().getStore());
	EXPECT_EQ(a().getState(), model::gameobjects::state::getId(CreatureState::ACTIVE)) << "PRIVATE_SHOP unset, then ACTIVE set";
	std::vector<std::vector<uint8_t>> own = ofOpcode(sent(), SM_EMOTION_OPCODE);
	ASSERT_EQ(own.size(), 1u);
	int32_t sender = 0;
	EXPECT_EQ(emotionOf(own[0], sender), CLOSE_PRIVATESHOP);
	EXPECT_EQ(ofOpcode(sentB(), SM_EMOTION_OPCODE).size(), 1u);

	clearSent();
	PrivateStoreService::closePrivateStore(a());
	EXPECT_TRUE(sent().empty()) << ":116-117: no store, nothing";
}

// :52-83, one refusal each, in Java's order of checks
TEST_F(PrivateStoreTest, AFlyingPlayerCannotOpenAStore) {
	a().setFlyState(model::gameobjects::state::FlyState::FLYING);
	expectRefused(cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_FLY_MODE())}));
}

TEST_F(PrivateStoreTest, AMovingPlayerCannotOpenAStore) {
	a().getMoveController()->setInMove(true);
	expectRefused(cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_MOVING_OBJECT())}));
}

TEST_F(PrivateStoreTest, APlayerInAttackModeCannotOpenAStore) {
	a().setState(CreatureState::WEAPON_EQUIPPED); // Player.isInAttackMode
	expectRefused(cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_COMBAT_MODE())}));
}

TEST_F(PrivateStoreTest, ATradingPlayerCannotOpenAStore) {
	services::ExchangeService::getInstance().registerExchange(a(), partner());
	ASSERT_TRUE(a().isTrading());
	clearSent();
	expectRefused(cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_CANT_OPEN_STORE_DURING_CRAFTING())}));
}

TEST_F(PrivateStoreTest, APlayerInARobotCannotOpenAStore) {
	a().setRobotId(1); // the RIDE / robot arm: isInRobotMode
	expectRefused(cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_RESTRICTION_RIDE())}));
}

TEST_F(PrivateStoreTest, AHiddenPlayerCannotOpenAStore) {
	a().getEffectController()->setAbnormal(skillengine::effect::AbnormalState::HIDE);
	expectRefused(cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_DISABLED_IN_HIDDEN_MODE())}));
}

TEST_F(PrivateStoreTest, ADeadPlayerCannotOpenAStoreAndIsNotTold) {
	a().setLifeStats(std::make_unique<cp::DeadPlayerLifeStats>(a()));
	ASSERT_TRUE(a().isDead());
	expectRefused({});
}

TEST_F(PrivateStoreTest, APlayerOnAChairCannotOpenAStoreAndIsNotTold) {
	a().setState(CreatureState::CHAIR, true);
	expectRefused({});
}

TEST_F(PrivateStoreTest, ASecondStoreIsRefusedSilently) {
	open(rows({{SWORD, TRAINING_SWORD, 1, 5000}}));
	runtime::Ptr<model::gameobjects::player::PrivateStore> first = a().getStore();
	ASSERT_TRUE(first);
	clearSent();
	clearSentB();

	open(rows({{POTIONS, MINOR_LIFE_POTION, 10, 100}}));

	EXPECT_EQ(a().getStore().rawPointer(), first.rawPointer());
	EXPECT_EQ(storeRows(), (std::vector<std::array<int64_t, 3>>{{SWORD, 1, 5000}}));
	EXPECT_TRUE(sent().empty());
}

// :86-113, validateItem: a refused row drops the whole store, the rows before it included
TEST_F(PrivateStoreTest, ARowIsValidatedAgainstTheInventoryAndARefusalDropsTheWholeStore) {
	struct Row {
		std::array<int64_t, 4> row;
		std::vector<std::vector<uint8_t>> expected;
		const char* why;
	};
	// the sword is a valid first row in every request below
	const std::vector<Row> refused = {
		{{999999, MINOR_LIFE_POTION, 1, 100}, {}, ":87 no item of that object id"},
		{{POTIONS, TRAINING_SWORD, 1, 100}, {}, ":87 the item id does not match the item"},
		{{POTIONS, MINOR_LIFE_POTION, 101, 100}, {}, ":90 more than the stack"},
		{{POTIONS, MINOR_LIFE_POTION, 0, 100}, {}, ":90 a count below 1"},
		{{POTIONS, MINOR_LIFE_POTION, 1, -1}, {}, ":93 a negative price"},
		{{JUICE, FRUIT_JUICE, 1, 100}, cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_CANNOT_BE_EXCHANGED())}), ":100 not tradeable"},
		{{SWORD, TRAINING_SWORD, 1, 100}, cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_ALREAY_REGIST_ITEM())}), ":108 the same item twice"},
	};
	for (const Row& row : refused) {
		clearSent();
		open(rows({{SWORD, TRAINING_SWORD, 1, 5000}, row.row}));
		EXPECT_FALSE(a().getStore()) << row.why;
		EXPECT_EQ(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE), row.expected) << row.why;
		PrivateStoreService::closePrivateStore(a()); // each row on its own
	}

	// :104: an item flagged equipped in the cube
	a().getInventory().getItemByObjId(POTIONS)->setEquipped(true);
	clearSent();
	open(rows({{SWORD, TRAINING_SWORD, 1, 5000}, {POTIONS, MINOR_LIFE_POTION, 1, 100}}));
	EXPECT_FALSE(a().getStore()) << ":104 equipped";
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_CAN_NOT_SELL_EQUIPED_ITEM())}));
	a().getInventory().getItemByObjId(POTIONS)->setEquipped(false);

	// :100: an untradeable item still in its package (pack count > 0) is accepted
	a().getInventory().getItemByObjId(JUICE)->setPackCount(1);
	clearSent();
	open(rows({{JUICE, FRUIT_JUICE, 1, 100}}));
	EXPECT_TRUE(a().getStore()) << "packed: tradeable until unpacked";
}

// :96-98: the eleventh row
TEST_F(PrivateStoreTest, TheEleventhRowFindsTheBasketFull) {
	std::vector<runtime::Ref<TradePSItem>> request;
	for (int32_t i = 0; i < 11; i++) {
		give(a(), 810000 + i, TRAINING_SWORD, 1);
		request.push_back(TradePSItem::create(810000 + i, TRAINING_SWORD, 1, 10));
	}
	std::vector<runtime::Ptr<TradePSItem>> ten(request.begin(), request.begin() + 10);
	PrivateStoreService::createStoreWithItems(a(), ten);
	ASSERT_TRUE(a().getStore()) << "ten rows fit";
	PrivateStoreService::closePrivateStore(a());
	clearSent();

	open(request);

	EXPECT_FALSE(a().getStore());
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_PERSONAL_SHOP_FULL_BASKET())}));
}

// :228-231
TEST_F(PrivateStoreTest, TheStoreNameIsStoredAndBroadcast) {
	open(rows({{POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	clearSent();
	clearSentB();

	PrivateStoreService::openPrivateStore(a(), "Cheap potions");

	EXPECT_EQ(a().getStore()->getStoreMessage(), "Cheap potions");
	// SM_PRIVATE_STORE_NAME.java writeImpl: D(playerObjId), S(name)
	std::vector<uint8_t> name = itemtest::javaPacket(SM_PRIVATE_STORE_NAME_OPCODE, PacketWriter().D(a().getObjectId()).S("Cheap potions"));
	EXPECT_EQ(sent(), cp::exactly({name}));
	EXPECT_EQ(sentB(), cp::exactly({name}));

	PrivateStoreService::closePrivateStore(a());
	EXPECT_THROW(PrivateStoreService::openPrivateStore(a(), "x"), runtime::NullPointerException) << "Java: getStore() is null";
}

// :127-181, :189-195, :202-223: the index semantics and the ledger
TEST_F(PrivateStoreTest, ABuyerBuysByStoreIndexAndTheKinahMovesOnce) {
	open(rows({{SWORD, TRAINING_SWORD, 1, 5000}, {POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	clearSent();
	clearSentB();

	buy({{1, 10}}); // index 1 is the second row: the potions

	EXPECT_EQ(kinah(partner()), 10000 - 10 * 100);
	EXPECT_EQ(kinah(a()), 1000 + 10 * 100);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 90);
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 10);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION) + countOf(partner(), MINOR_LIFE_POTION), 100) << "conserved";
	EXPECT_EQ(storeRows(), (std::vector<std::array<int64_t, 3>>{{SWORD, 1, 5000}, {POTIONS, 20, 100}}));
	runtime::Ptr<Item> potions = a().getInventory().getItemByObjId(POTIONS);
	std::vector<std::vector<uint8_t>> sellerMessages = ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE);
	EXPECT_EQ(sellerMessages, cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI(10, potions->getL10n()))}));

	// index 0 is the sword (count 1: the single-item message); it sells out and leaves the store
	std::string swordName = a().getInventory().getItemByObjId(SWORD)->getL10n();
	clearSent();
	buy({{0, 1}});
	EXPECT_EQ(kinah(partner()), 10000 - 1000 - 5000);
	EXPECT_EQ(kinah(a()), 1000 + 1000 + 5000);
	EXPECT_FALSE(a().getInventory().getItemByObjId(SWORD));
	EXPECT_EQ(countOf(partner(), TRAINING_SWORD), 1);
	EXPECT_FALSE(partner().getInventory().getItemByObjId(SWORD)) << "the buyer gets a new item (ItemService.addItem(buyer, item, count))";
	EXPECT_EQ(storeRows(), (std::vector<std::array<int64_t, 3>>{{POTIONS, 20, 100}}));
	sellerMessages = ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE);
	EXPECT_EQ(sellerMessages, cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_SELL_ITEM(swordName))}));

	// the sword's row is gone: the potions are index 0 now, and buying the rest closes the store
	clearSent();
	clearSentB();
	buy({{0, 20}});
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 70);
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 30);
	EXPECT_EQ(kinah(a()), 1000 + 1000 + 5000 + 2000);
	EXPECT_FALSE(a().getStore()) << ":179-180: sold out closes the store";
	std::vector<std::vector<uint8_t>> emotions = ofOpcode(sentB(), SM_EMOTION_OPCODE);
	ASSERT_EQ(emotions.size(), 1u);
	int32_t sender = 0;
	EXPECT_EQ(emotionOf(emotions[0], sender), CLOSE_PRIVATESHOP);
}

// :209-219: getBoughtItems' refusals - an index outside the store (the object id is not an index), more than is for sale
TEST_F(PrivateStoreTest, AnInvalidIndexOrTooLargeACountBuysNothing) {
	open(rows({{POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	network::test::LogCapture capture({EXCHANGE_LOGGER});
	clearSent();
	clearSentB();

	buy({{POTIONS, 1}});
	buy({{1, 1}});
	buy({{-1, 1}});
	buy({{0, 31}});
	buy({{0, 1}, {1, 1}});  // one invalid index spoils the whole list
	buy({{0, 1}, {0, 31}}); // and so does one count too large

	EXPECT_EQ(capture.count("[Private Store] Attempt to buy from invalid store index: " + std::to_string(POTIONS)), 1) << capture.dump();
	EXPECT_EQ(capture.count("[Private Store] Attempt to buy from invalid store index: 1"), 2) << capture.dump();
	EXPECT_EQ(capture.count("[Private Store] Attempt to buy from invalid store index: -1"), 1) << capture.dump();
	EXPECT_EQ(capture.count("[Private Store] Attempt to buy more than for sale: 31 vs. 30"), 2) << capture.dump();
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(kinah(partner()), 10000);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(sentB().empty());
}

// :135-150: the buyer's slots and kinah; the price as a Java long (:141-147)
TEST_F(PrivateStoreTest, TheBuyerNeedsTheSlotsAndExactlyEnoughKinah) {
	open(rows({{POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	clearSent();
	clearSentB();

	// exact kinah: 25 potions cost 2,500; 2,499 is refused, 2,500 buys (price > kinah is the refusal, :149)
	partner().getInventory().decreaseKinah(10000 - 2499);
	clearSentB();
	buy({{0, 25}});
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(kinah(partner()), 2499);
	EXPECT_TRUE(sentB().empty());
	partner().getInventory().increaseKinah(1);
	buy({{0, 25}});
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 25);
	EXPECT_EQ(kinah(partner()), 0);

	// a full cube: STR_MSG_DICE_INVEN_ERROR
	partner().getInventory().increaseKinah(1000);
	for (int32_t objId = 820000; partner().getInventory().getFreeSlots() > 0; objId++)
		give(partner(), objId, TRAINING_SWORD, 1);
	clearSentB();
	buy({{0, 1}});
	EXPECT_EQ(sentB(), cp::exactly({serializedForB(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())}));
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 25);
}

// :135-150, :162-176: one CM_BUY_ITEM of two rows - the buyer needs a free slot per row (one short is refused, exactly as many buys) and
// pays the sum of the rows once (1 x 5,000 + 10 x 100); each row moves and is announced to the seller
TEST_F(PrivateStoreTest, TwoRowsInOneListNeedTwoFreeSlotsAndCostTheirSum) {
	constexpr int32_t FIRST_FILLER = 820000;
	open(rows({{SWORD, TRAINING_SWORD, 1, 5000}, {POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	for (int32_t objId = FIRST_FILLER; partner().getInventory().getFreeSlots() > 1; objId++)
		give(partner(), objId, TRAINING_SWORD, 1);
	std::string swordName = a().getInventory().getItemByObjId(SWORD)->getL10n();
	std::string potionName = a().getInventory().getItemByObjId(POTIONS)->getL10n();
	clearSent();
	clearSentB();

	buy({{0, 1}, {1, 10}});

	EXPECT_EQ(sentB(), cp::exactly({serializedForB(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())})) << "one free slot for two rows";
	EXPECT_EQ(kinah(partner()), 10000);
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	EXPECT_TRUE(a().getInventory().getItemByObjId(SWORD));
	EXPECT_TRUE(sent().empty());

	partner().getInventory().delete_(*partner().getInventory().getItemByObjId(FIRST_FILLER));
	ASSERT_EQ(partner().getInventory().getFreeSlots(), 2);
	clearSentB();
	buy({{0, 1}, {1, 10}});

	EXPECT_EQ(kinah(partner()), 10000 - (1 * 5000 + 10 * 100)) << "the sum of both rows, taken once";
	EXPECT_EQ(kinah(a()), 1000 + 1 * 5000 + 10 * 100);
	EXPECT_EQ(kinah(a()) + kinah(partner()), 1000 + 10000) << "conserved";
	EXPECT_FALSE(a().getInventory().getItemByObjId(SWORD));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 90);
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 10);
	EXPECT_EQ(partner().getInventory().getFreeSlots(), 0) << "a new sword and a new potion stack";
	EXPECT_EQ(storeRows(), (std::vector<std::array<int64_t, 3>>{{POTIONS, 20, 100}}));
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE),
		cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_SELL_ITEM(swordName)),
			serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI(10, potionName))}));
}

TEST_F(PrivateStoreTest, APriceThatOverflowsJavasLongIsAudited) {
	// 2^62 per potion: 2 potions are 2^63, which wraps to Long.MIN_VALUE (:141-142) and hits the kinah dupe guard (:144-147)
	open(rows({{POTIONS, MINOR_LIFE_POTION, 30, int64_t{1} << 62}}));
	network::test::LogCapture capture({AUDIT_LOGGER});
	clearSentB();

	buy({{0, 2}});

	EXPECT_TRUE(capture.contains("tried to buy item with negative kinah price from private store")) << capture.dump();
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(kinah(partner()), 10000);
	EXPECT_EQ(kinah(a()), 1000);
}

// :152-160: the seller used some of a listed stack; Java returns from inside the loop, after the rows before it moved and before any kinah
// moves - the earlier rows go to the buyer unpaid (pinned as Java has it, docs/deviations/P5-09b.md "M5c stage 1")
TEST_F(PrivateStoreTest, AStackTheSellerUsedUpStopsTheSaleHalfWayAsInJava) {
	give(a(), MORE_POTIONS, MINOR_LIFE_POTION, 30);
	open(rows({{POTIONS, MINOR_LIFE_POTION, 10, 100}, {MORE_POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	a().getInventory().decreaseItemCount(*a().getInventory().getItemByObjId(MORE_POTIONS), 25); // 5 left, 30 still listed
	network::test::LogCapture capture({AUDIT_LOGGER});

	buy({{0, 10}, {1, 10}});

	EXPECT_TRUE(capture.contains("tried to buy more than players private store item stack count")) << capture.dump();
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 10) << "the first row moved";
	EXPECT_EQ(kinah(partner()), 10000) << "and was not paid for";
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 90 + 5);
}

// :128-129
TEST_F(PrivateStoreTest, AnOfflineSellerOrBuyerOrAnotherRaceBuysNothing) {
	open(rows({{POTIONS, MINOR_LIFE_POTION, 30, 100}}));

	b.commonData->setRace(model::Race::ASMODIANS);
	buy({{0, 1}});
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	b.commonData->setRace(model::Race::ELYOS);

	partner().setClientConnection(nullptr);
	buy({{0, 1}});
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	clientB->enterWorld(b);

	a().setClientConnection(nullptr);
	buy({{0, 1}});
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	client->enterWorld(f);

	buy({{0, 1}});
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 1) << "the control: both online and of one race";
}

// W-25: the buyer's new sword is a copy of the seller's (ItemService.copyItemInfo), its mana stone included (ItemSocketService.addManaStone)
TEST_F(PrivateStoreTest, ASocketedItemIsSoldWithItsManaStone) {
	constexpr int32_t SOCKETED = 800020;
	Item& sword = give(a(), SOCKETED, SLOT_TEST_SWORD, 1);
	ASSERT_TRUE(services::item::ItemSocketService::addManaStone(runtime::Ptr<Item>(sword), MANASTONE_HP_20, false));
	open(rows({{SOCKETED, SLOT_TEST_SWORD, 1, 700}}));

	buy({{0, 1}});

	EXPECT_FALSE(a().getInventory().getItemByObjId(SOCKETED));
	std::vector<runtime::Ptr<Item>> bought = partner().getInventory().getItemsByItemId(SLOT_TEST_SWORD);
	ASSERT_EQ(bought.size(), 1u);
	EXPECT_NE(bought[0]->getObjectId(), SOCKETED);
	ASSERT_TRUE(bought[0]->hasManaStones());
	auto stones = bought[0]->getItemStones()->snapshot();
	ASSERT_EQ(stones.size(), 1u);
	EXPECT_EQ(stones[0]->getItemId(), MANASTONE_HP_20);
	EXPECT_EQ(stones[0]->getItemObjId(), bought[0]->getObjectId()) << "the stone belongs to the new item";
	EXPECT_EQ(kinah(a()), 1000 + 700);
}

} // namespace
} // namespace aion::gameserver::economy::test::trade
