// M5c P-03 (m5c-plan.md §5, P5-07): RepurchaseService.repurchaseFromShop, the buy-back tab of a merchant (CM_BUY_ITEM's repurchase arm,
// CM_BUY_ITEM.java:124), against RepurchaseService.java:47-69. Tests of P-06.
//
// The sold items are the ones TradeService keeps for the seller (addRepurchaseItems) with the repurchase price its sell arm sets
// (item.setRepurchasePrice(realReward), the kinah the sale paid): tools/oracle `oracle.py m5c-trade --npc 798007 --item 182004793 --count 3
// --no-profile --set gameserver.siege.enable=false` gives 180 for 3 Sparkie Carapace Fragments (price 300, vendor sell modifier 20) and the same
// command with `--item 162000002 --count 5` gives 250 for 5 Minor Life Potions (price 250), and with `--item 100001551 --count 1` 537,460 for
// Modor's Sword (price 2,687,303). The item rows are the verbatim ones of ItemServicesTestSupport.h and PlayerItemsTestSupport.h. The packets
// are compared against the server's own serialization of the factories Java calls (their bytes are pinned by the sm tests).

#include "PlayerItemsTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/trade/RepurchaseList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test::playeritems {
namespace {

using network::aion::serverpackets::SM_INVENTORY_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t MINALINERK_OBJECT_ID = 910001; // the merchant's object id in the list (RepurchaseList.sellerObjId): not read by this body
constexpr int64_t FRAGMENTS_PRICE = 180;         // oracle m5c-trade, 3 fragments sold to 798007
constexpr int64_t POTIONS_PRICE = 250;           // oracle m5c-trade, 5 potions sold to 798007
constexpr int64_t SWORD_PRICE = 537460;          // oracle m5c-trade, Modor's Sword sold to 798007 (sell modifier 20)

class RepurchaseServiceTest : public PlayerItemsTest {
protected:
	void TearDown() override {
		if (f.player)
			RepurchaseService::getInstance().removeRepurchaseItems(*f.player); // the singleton outlives the case
		PlayerItemsTest::TearDown();
	}

	/** A sold stack: not in any storage any more, with the repurchase price the sale set */
	Item& sold(int32_t objId, int32_t itemId, int64_t count, int64_t repurchasePrice) {
		Item& item = loose(objId, itemId, count);
		item.setRepurchasePrice(repurchasePrice);
		return item;
	}

	/** Java CM_BUY_ITEM's list: RepurchaseList.addRepurchaseItem of each object id, which keeps only the ones the seller may repurchase */
	Ref<model::trade::RepurchaseList> listOf(std::initializer_list<int32_t> itemObjectIds) {
		Ref<model::trade::RepurchaseList> list = model::trade::RepurchaseList::create(MINALINERK_OBJECT_ID);
		for (int32_t itemObjectId : itemObjectIds)
			list->addRepurchaseItem(player(), itemObjectId, 1);
		return list;
	}

	std::vector<int32_t> repurchasableIds() {
		std::vector<int32_t> ids;
		for (const Ptr<Item>& item : RepurchaseService::getInstance().getRepurchaseItems(player().getObjectId()))
			ids.push_back(item->getObjectId());
		std::sort(ids.begin(), ids.end());
		return ids;
	}

	int64_t cubeCount(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }

	std::vector<uint8_t> systemMessage(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }
};

TEST_F(RepurchaseServiceTest, EachListedItemIsBoughtBackForItsRepurchasePrice) {
	// RepurchaseService.java:52-66: for each object id of the list, in its order, the item is found among the sold ones, its repurchase price
	// is taken (tryDecreaseKinah) and ItemService.addItem puts a copy into the cube; then it leaves the sold items
	giveKinah(940001, 1000);
	Item& fragments = sold(940011, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	Item& potions = sold(940012, MINOR_LIFE_POTION, 5, POTIONS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments), Ptr<Item>(potions)});
	Ref<model::trade::RepurchaseList> list = listOf({940012, 940011});
	ASSERT_EQ(list->size(), 2);

	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);

	EXPECT_EQ(player().getInventory().getKinah(), 1000 - POTIONS_PRICE - FRAGMENTS_PRICE);
	EXPECT_EQ(cubeCount(SPARKIE_CARAPACE_FRAGMENT), 3);
	EXPECT_EQ(cubeCount(MINOR_LIFE_POTION), 5);
	EXPECT_TRUE(repurchasableIds().empty());
	EXPECT_TRUE(sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE).empty()) << "no refusal";
}

TEST_F(RepurchaseServiceTest, WithoutEnoughKinahTheItemStaysSoldAndTheAttemptIsAudited) {
	// RepurchaseService.java:59-65: tryDecreaseKinah(price) fails -> AuditLogger.log(player, "tried to repurchase item <id>, count: <count>
	// without kinah"), nothing is added and the item stays among the sold ones; the next item of the list is still tried, and a purse that holds
	// exactly its price buys it (Storage.tryDecreaseKinah: kinah >= amount)
	network::test::LogCapture audit({"AUDIT_LOG"});
	const bool savedLogAudit = configs::main::LoggingConfig::LOG_AUDIT.exchange(true);
	giveKinah(940101, FRAGMENTS_PRICE);
	Item& fragments = sold(940111, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	Item& potions = sold(940112, MINOR_LIFE_POTION, 5, POTIONS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments), Ptr<Item>(potions)});
	Ref<model::trade::RepurchaseList> list = listOf({940112, 940111});

	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);
	configs::main::LoggingConfig::LOG_AUDIT.store(savedLogAudit);

	EXPECT_EQ(audit.count("tried to repurchase item 162000002, count: 5 without kinah"), 1) << audit.dump();
	EXPECT_EQ(audit.count("tried to repurchase item"), 1) << audit.dump();
	EXPECT_EQ(cubeCount(MINOR_LIFE_POTION), 0);
	EXPECT_EQ(cubeCount(SPARKIE_CARAPACE_FRAGMENT), 3);
	EXPECT_EQ(player().getInventory().getKinah(), 0) << "the exact price bought the fragments";
	EXPECT_EQ(repurchasableIds(), (std::vector<int32_t>{940112}));
}

TEST_F(RepurchaseServiceTest, AFullCubeStopsTheRepurchaseWithTheDiceMessage) {
	// RepurchaseService.java:53-56: the cube is checked before each item; once it is full, STR_MSG_DICE_INVEN_ERROR and the loop ends. A cube
	// of one slot takes the first item of the list and refuses the second
	giveKinah(940201, 1000);
	Item& fragments = sold(940211, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	Item& potions = sold(940212, MINOR_LIFE_POTION, 5, POTIONS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments), Ptr<Item>(potions)});
	Ref<model::trade::RepurchaseList> list = listOf({940211, 940212});
	Ref<model::trade::RepurchaseList> fragmentsOnly = listOf({940211}); // built while the fragments are still sold (canRepurchase)
	player().getInventory().setLimit(1);

	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);

	EXPECT_EQ(cubeCount(SPARKIE_CARAPACE_FRAGMENT), 3);
	EXPECT_EQ(cubeCount(MINOR_LIFE_POTION), 0);
	EXPECT_EQ(player().getInventory().getKinah(), 1000 - FRAGMENTS_PRICE);
	EXPECT_EQ(repurchasableIds(), (std::vector<int32_t>{940212}));
	EXPECT_EQ(sentWithOpcode(SM_SYSTEM_MESSAGE_OPCODE), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())}));

	// a cube that is full from the start refuses at once, before any lookup: the only listed id (the fragments, bought back above) is no longer
	// among the sold items, so a lookup first would find nothing and send nothing
	clearSent();
	RepurchaseService::getInstance().repurchaseFromShop(player(), *fragmentsOnly);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())}));
	EXPECT_EQ(player().getInventory().getKinah(), 1000 - FRAGMENTS_PRICE);
}

TEST_F(RepurchaseServiceTest, AFullCubeRefusesBeforeTheSoldItemsAreRead) {
	// RepurchaseService.java:52-56: `items` is null once removeRepurchaseItems ran (logout), but the full cube sends STR_MSG_DICE_INVEN_ERROR and
	// breaks before the first lookup (:58) could throw its NullPointerException
	giveKinah(940251, 1000);
	Item& fragments = sold(940261, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments)});
	Ref<model::trade::RepurchaseList> list = listOf({940261});
	RepurchaseService::getInstance().removeRepurchaseItems(player());
	player().getInventory().setLimit(0);

	EXPECT_NO_THROW(RepurchaseService::getInstance().repurchaseFromShop(player(), *list));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())}));
	EXPECT_EQ(player().getInventory().getKinah(), 1000);
}

TEST_F(RepurchaseServiceTest, ABoughtBackSwordIsANewItemWithTheSoldOnesEnchantmentStonesAndIdentity) {
	// RepurchaseService.java:61: ItemService.addItem(player, repurchaseItem) is the source-item overload (ItemService.java:49-51): Modor's
	// Sword is not stackable, so addNonStackableItem (:103-119) makes a new item and copyItemInfo (:123-143) copies the sold one's optional
	// sockets, mana stones, godstone, enchant level and bonus, tempering, soul binding, tune count and bonus stats (addItem(player, itemId,
	// count) would hand back a blank sword). The kinah is taken with Storage.tryDecreaseKinah(amount), whose update type is DEC_KINAH_BUY
	// (Storage.java:82-103); the copy is added as DEFAULT_UPDATE_PREDICATE's ItemAddType.ITEM_COLLECT, then SM_CUBE_UPDATE.cubeSize (the
	// kinah is not among the cube's items)
	giveKinah(940601, 600000);
	Item& sword = sold(940611, MODORS_SWORD, 1, SWORD_PRICE);
	sword.setOptionalSockets(1);
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_20, false));
	ASSERT_TRUE(ItemSocketService::addManaStone(Ptr<Item>(sword), MANASTONE_HP_20, false));
	sword.addGodStone(FX_TEST_EARTH_GODSTONE, 2);
	sword.setEnchantLevel(10);
	sword.setEnchantBonus(2);
	sword.setTempering(3);
	sword.setSoulBound(true);
	sword.setTuneCount(1); // after the enchant level: Item.setEnchantLevel may raise the remaining tune count (Item.java removeRemainingTuningCountIfPossible)
	sword.setBonusStats(3, false);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(sword)});
	Ref<model::trade::RepurchaseList> list = listOf({940611});
	clearSent();

	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);

	std::vector<Ptr<Item>> swords = player().getInventory().getItemsByItemId(MODORS_SWORD);
	ASSERT_EQ(swords.size(), 1u);
	Item& copy = *swords[0];
	EXPECT_NE(copy.getObjectId(), 940611) << "a new item (ItemFactory.newItem), not the sold one";
	EXPECT_EQ(copy.getItemCount(), 1);
	EXPECT_EQ(copy.getOptionalSockets(), 1);
	// item id, slot: a plain MANASTONE goes after the sword's one special slot (s_slots="1", ItemSocketService.java:72), in the copy as in the source
	std::vector<std::pair<int32_t, int32_t>> stones;
	for (const Ptr<model::items::ManaStone>& stone : copy.getItemStones()->snapshot())
		stones.emplace_back(stone->getItemId(), stone->getSlot());
	EXPECT_EQ(stones, (std::vector<std::pair<int32_t, int32_t>>{{MANASTONE_HP_20, 1}, {MANASTONE_HP_20, 2}}));
	ASSERT_TRUE(copy.getGodStone());
	EXPECT_EQ(copy.getGodStone()->getItemId(), FX_TEST_EARTH_GODSTONE);
	EXPECT_EQ(copy.getGodStone()->getActivatedCount(), 2);
	EXPECT_EQ(copy.getEnchantLevel(), 10);
	EXPECT_EQ(copy.getEnchantBonus(), 2);
	EXPECT_EQ(copy.getTempering(), 3);
	EXPECT_TRUE(copy.isSoulBound());
	EXPECT_EQ(copy.getTuneCount(), 1);
	EXPECT_EQ(copy.getBonusStatsId(), 3);
	EXPECT_EQ(player().getInventory().getKinah(), 600000 - SWORD_PRICE);
	EXPECT_TRUE(repurchasableIds().empty());
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_UPDATE_ITEM(player(), *player().getInventory().getKinahItem(),
									   ItemPacketService::ItemUpdateType::DEC_KINAH_BUY)),
						  serialized(SM_INVENTORY_ADD_ITEM({swords[0]}, player(), ItemPacketService::ItemAddType::ITEM_COLLECT)),
						  cubeSize(StorageType::CUBE, 1)}));
}

TEST_F(RepurchaseServiceTest, AnItemNoLongerAmongTheSoldOnesIsSkipped) {
	// RepurchaseService.java:58: findAny().orElse(null) -> nothing for an id the seller no longer has (bought back already); the list of a
	// buy-back is kept by the client, so the same ids come again
	giveKinah(940301, 1000);
	Item& fragments = sold(940311, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments)});
	Ref<model::trade::RepurchaseList> list = listOf({940311});
	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);
	ASSERT_EQ(cubeCount(SPARKIE_CARAPACE_FRAGMENT), 3);
	clearSent();

	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);

	EXPECT_EQ(cubeCount(SPARKIE_CARAPACE_FRAGMENT), 3) << "not bought a second time";
	EXPECT_EQ(player().getInventory().getKinah(), 1000 - FRAGMENTS_PRICE);
	EXPECT_TRUE(sent().empty());
}

TEST_F(RepurchaseServiceTest, APlayerWhoCannotTradeRepurchasesNothing) {
	// RepurchaseService.java:48-50: PlayerRestrictions.canTrade (a dead player, PlayerRestrictions.java:241) -> return before anything
	giveKinah(940401, 1000);
	Item& fragments = sold(940411, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments)});
	Ref<model::trade::RepurchaseList> list = listOf({940411});
	player().getInventory().setLimit(0); // a full cube would send STR_MSG_DICE_INVEN_ERROR if the loop ran
	player().setLifeStats(std::make_unique<cp::DeadPlayerLifeStats>(player()));

	RepurchaseService::getInstance().repurchaseFromShop(player(), *list);

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(player().getInventory().getKinah(), 1000);
	EXPECT_EQ(repurchasableIds(), (std::vector<int32_t>{940411}));
}

TEST_F(RepurchaseServiceTest, AfterTheSoldItemsWereDroppedTheLookupIsJavasNullPointer) {
	// RepurchaseService.java:52, :58: repurchaseItems.get(objectId) is null once removeRepurchaseItems ran (logout), and items.stream() throws
	// NullPointerException for the first listed id
	giveKinah(940501, 1000);
	Item& fragments = sold(940511, SPARKIE_CARAPACE_FRAGMENT, 3, FRAGMENTS_PRICE);
	RepurchaseService::getInstance().addRepurchaseItems(player(), {Ptr<Item>(fragments)});
	Ref<model::trade::RepurchaseList> list = listOf({940511});
	RepurchaseService::getInstance().removeRepurchaseItems(player());

	EXPECT_THROW(RepurchaseService::getInstance().repurchaseFromShop(player(), *list), runtime::NullPointerException);
	EXPECT_EQ(player().getInventory().getKinah(), 1000);
	EXPECT_EQ(cubeCount(SPARKIE_CARAPACE_FRAGMENT), 0);

	// an empty list never reaches the lookup
	EXPECT_NO_THROW(RepurchaseService::getInstance().repurchaseFromShop(player(), *listOf({})));
}

} // namespace
} // namespace aion::gameserver::services::item::test::playeritems
