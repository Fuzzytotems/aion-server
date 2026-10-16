// P5-07 ItemService (m5b3-plan.md T-01, T-07): every addItem overload, addStackableItem with its POWER_SHARDS equipment arm,
// addNonStackableItem, copyItemInfo, ItemUpdatePredicate.getUpdateType (ItemService.java:34-208), and the companion headers of
// ItemPacketService's three nested enums (ItemPacketService.java:27-152), against Java's constants and packets.

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/dataholders/ItemRandomBonusData.bind.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddTypeInfo.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteTypeInfo.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateTypeInfo.h"
#include "aion/gameserver/services/item/ItemService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using network::aion::serverpackets::SM_INVENTORY_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ItemAddType = ItemPacketService::ItemAddType;
using ItemDeleteType = ItemPacketService::ItemDeleteType;
using ItemUpdateType = ItemPacketService::ItemUpdateType;

/** ItemSlot.POWER_SHARD_RIGHT's slot id mask (ItemSlot.java: 1 << 13) */
constexpr int64_t POWER_SHARD_RIGHT = 1LL << 13;

class ItemServiceTest : public ItemServicesTest {
protected:
	/** The items of the cube with this template id that are not among `known` (the ones addItem created) */
	std::vector<Ptr<Item>> newItemsOf(int32_t itemId, const std::vector<int32_t>& known = {}) {
		std::vector<Ptr<Item>> created;
		for (const Ptr<Item>& item : player().getInventory().getItemsByItemId(itemId)) {
			if (std::find(known.begin(), known.end(), item->getObjectId()) == known.end())
				created.push_back(item);
		}
		return created;
	}

	/** An SM_INVENTORY_ADD_ITEM of one item: writeH(addType mask), writeH(1), then the item's writeD(objectId), writeD(itemId) (the blob follows) */
	void expectAddItem(const std::vector<uint8_t>& packet, Item& item, int32_t addTypeMask) {
		ASSERT_EQ(javaOpcodeOf(packet), SM_INVENTORY_ADD_ITEM_OPCODE);
		PacketReader reader(cp::bodyOf(packet));
		EXPECT_EQ(reader.H(), addTypeMask);
		EXPECT_EQ(reader.H(), 1);
		EXPECT_EQ(reader.D(), item.getObjectId());
		EXPECT_EQ(reader.D(), item.getItemId());
	}

	/** An SM_INVENTORY_UPDATE_ITEM: writeD(objectId) first and, for a sendable update type, writeH(mask) last */
	void expectUpdateItem(const std::vector<uint8_t>& packet, Item& item, int32_t updateTypeMask) {
		ASSERT_EQ(javaOpcodeOf(packet), SM_INVENTORY_UPDATE_ITEM_OPCODE);
		EXPECT_EQ(PacketReader(cp::bodyOf(packet)).D(), item.getObjectId());
		EXPECT_EQ(trailingMask(packet), updateTypeMask);
	}
};

// ------------------------------------------------------------------------------------------------------------------------- stackable items

TEST_F(ItemServiceTest, AddItemMergesIntoTheStackOfTheCubeAndSendsTheUpdateAlone) {
	// ItemService.java:38-40 -> :70-98 -> addStackableItem :148-178: the cube's stack of that id takes the count with
	// predicate.getUpdateType(item, true) = INC_ITEM_COLLECT (DEFAULT_UPDATE_PREDICATE, :31-32) -> Storage.increaseItemCount ->
	// ItemPacketService.sendItemPacket: SM_INVENTORY_UPDATE_ITEM alone (m5b3-plan.md Y3's merge arm)
	Item& potions = stored(810101, MINOR_LIFE_POTION, 100);
	EXPECT_EQ(ItemService::addItem(player(), MINOR_LIFE_POTION, 5), 0) << "nothing left over";
	EXPECT_EQ(potions.getItemCount(), 105);
	EXPECT_EQ(player().getInventory().size(), 1) << "no new stack";
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 1u) << "no SM_CUBE_UPDATE after a merge";
	expectUpdateItem(packets[0], potions, 0x19);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), potions, ItemUpdateType::INC_ITEM_COLLECT)));
}

TEST_F(ItemServiceTest, AddItemOfAnIdTheCubeDoesNotHoldMakesANewStackWithAddThenCubeSize) {
	// addStackableItem :172-176: ItemFactory.newItem(id, count), inventory.add(newItem, ITEM_COLLECT) -> Storage.add ->
	// ItemPacketService.sendStorageUpdatePacket: SM_INVENTORY_ADD_ITEM(ITEM_COLLECT 0x19), then SM_CUBE_UPDATE.cubeSize (m5b3-plan.md Y3)
	EXPECT_EQ(ItemService::addItem(player(), SPARKIE_CARAPACE_FRAGMENT, 3), 0);
	std::vector<Ptr<Item>> created = newItemsOf(SPARKIE_CARAPACE_FRAGMENT);
	ASSERT_EQ(created.size(), 1u);
	EXPECT_EQ(created[0]->getItemCount(), 3);
	EXPECT_EQ(created[0]->getItemLocation(), 0) << "CUBE";
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	expectAddItem(packets[0], *created[0], 0x19);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_ADD_ITEM({created[0]}, player(), ItemAddType::ITEM_COLLECT)));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 1));
}

TEST_F(ItemServiceTest, AnOverflowFillsTheStackThenMakesNewStacksOfAtMostTheMaxStackCount) {
	// Item.increaseItemCount (Item.java:315-326) caps the stack at max_stack_count 1000 (item_templates.xml:830724) and returns the rest; the
	// while loop of addStackableItem (:172-176) then makes stacks of ItemFactory.newItem's capped count (1000, then 10)
	Item& potions = stored(810102, MINOR_LIFE_POTION, 990);
	EXPECT_EQ(ItemService::addItem(player(), MINOR_LIFE_POTION, 1020), 0);
	EXPECT_EQ(potions.getItemCount(), 1000);
	std::vector<Ptr<Item>> created = newItemsOf(MINOR_LIFE_POTION, {810102});
	ASSERT_EQ(created.size(), 2u);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 5u);
	expectUpdateItem(packets[0], potions, 0x19);
	// the two new stacks in creation order: the first ADD names the full one
	PacketReader first(cp::bodyOf(packets[1]));
	first.H();
	first.H();
	const int32_t firstObjectId = first.D();
	Ptr<Item> full = created[0]->getObjectId() == firstObjectId ? created[0] : created[1];
	Ptr<Item> rest = created[0]->getObjectId() == firstObjectId ? created[1] : created[0];
	EXPECT_EQ(full->getItemCount(), 1000);
	EXPECT_EQ(rest->getItemCount(), 10);
	expectAddItem(packets[1], *full, 0x19);
	EXPECT_EQ(packets[2], cubeSize(StorageType::CUBE, 2));
	expectAddItem(packets[3], *rest, 0x19);
	EXPECT_EQ(packets[4], cubeSize(StorageType::CUBE, 3));
}

TEST_F(ItemServiceTest, AFullCubeKeepsTheCountAndSendsDiceInvenErrorUnlessOverflowIsAllowed) {
	// addStackableItem's loop runs only while `allowInventoryOverflow || !inventory.isFull(extraInventoryId)` (:172); what is left makes
	// addItem send STR_MSG_DICE_INVEN_ERROR and return it (:94-97, m5b3-plan.md §8 risk 5). The cube holds 27 (StorageType.java: CUBE limit 27).
	for (int32_t i = 0; i < 27; i++)
		stored(810200 + i, TRAINING_SWORD, 1);
	ASSERT_TRUE(player().getInventory().isFull());
	EXPECT_EQ(ItemService::addItem(player(), SPARKIE_CARAPACE_FRAGMENT, 4), 4) << "nothing was added";
	EXPECT_TRUE(newItemsOf(SPARKIE_CARAPACE_FRAGMENT).empty());
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())}));

	// ItemService.java:34-36: addItem(player, id, count, allowInventoryOverflow = true) adds into the full cube
	clearSent();
	EXPECT_EQ(ItemService::addItem(player(), SPARKIE_CARAPACE_FRAGMENT, 4, true), 0);
	std::vector<Ptr<Item>> created = newItemsOf(SPARKIE_CARAPACE_FRAGMENT);
	ASSERT_EQ(created.size(), 1u);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u) << "the add and the cube size, no message: nothing is left";
	expectAddItem(packets[0], *created[0], 0x19);
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 28));
}

TEST_F(ItemServiceTest, APowerShardFillsTheEquippedShardFirstWithAStatsChangeThenTheCubeStack) {
	// addStackableItem :152-161: for ItemGroup.POWER_SHARDS (item_templates.xml:848722) the equipped shards of that id take the count first
	// (Equipment.increaseEquippedItemCount -> ItemPacketService.updateItemAfterInfoChange(player, item, STATS_CHANGE), Equipment.java), then the
	// cube's stacks (:163-170)
	Ref<Item> equippedShard = loadedItem(810301, MINOR_POWER_SHARD, 9990, StorageType::CUBE, POWER_SHARD_RIGHT, true);
	items.push_back(equippedShard);
	player().getEquipment().onLoadHandler(*equippedShard);
	ASSERT_EQ(player().getEquipment().getEquippedItemsByItemId(MINOR_POWER_SHARD).size(), 1u);
	Item& cubeShards = stored(810302, MINOR_POWER_SHARD, 5);

	EXPECT_EQ(ItemService::addItem(player(), MINOR_POWER_SHARD, 50), 0);
	EXPECT_EQ(equippedShard->getItemCount(), 10000) << "max_stack_count 10000: 10 went to the equipped shard";
	EXPECT_EQ(cubeShards.getItemCount(), 45) << "the other 40 to the cube's stack";
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	expectUpdateItem(packets[0], *equippedShard, 0x00);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), *equippedShard, ItemUpdateType::STATS_CHANGE)));
	expectUpdateItem(packets[1], cubeShards, 0x19);
	EXPECT_EQ(packets[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), cubeShards, ItemUpdateType::INC_ITEM_COLLECT)));
}

// ------------------------------------------------------------------------------------------------------------------------- kinah

TEST_F(ItemServiceTest, KinahGoesToTheKinahItemWithIncKinahCollectAndIsNeverAStack) {
	// ItemService.java:83-87: itemTemplate.isKinah() -> inventory.increaseKinah(count) (INC_KINAH_COLLECT, Storage.java) and return 0 -
	// no SM_INVENTORY_ADD_ITEM (m5b3-plan.md Y4)
	Item& kinah = stored(810310, KINAH, 1000);
	EXPECT_EQ(ItemService::addItem(player(), KINAH, 25), 0);
	EXPECT_EQ(kinah.getItemCount(), 1025);
	EXPECT_EQ(player().getInventory().getKinah(), 1025);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 1u);
	expectUpdateItem(packets[0], kinah, 0x1A);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), kinah, ItemUpdateType::INC_KINAH_COLLECT)));
}

TEST_F(ItemServiceTest, ANonPositiveCountAddsNothing) {
	// ItemService.java:71-72
	EXPECT_EQ(ItemService::addItem(player(), SPARKIE_CARAPACE_FRAGMENT, 0), 0);
	EXPECT_EQ(ItemService::addItem(player(), SPARKIE_CARAPACE_FRAGMENT, -3), 0);
	EXPECT_EQ(player().getInventory().size(), 0);
	EXPECT_TRUE(sent().empty());
	// :74-75: Objects.requireNonNull(itemTemplate, "No item with id " + itemId)
	EXPECT_THROW(ItemService::addItem(player(), 123, 1), runtime::NullPointerException);
}

// ------------------------------------------------------------------------------------------------------------------------- non-stackable items

TEST_F(ItemServiceTest, NonStackableItemsAreAddedOneByOneUntilTheCubeIsFull) {
	// addNonStackableItem (ItemService.java:103-118): one ItemFactory.newItem per count while the cube is not full, each added with the
	// predicate's add type; what is left makes addItem send STR_MSG_DICE_INVEN_ERROR (:94-95). The Training Sword has no max_stack_count
	// (item_templates.xml:375).
	for (int32_t i = 0; i < 25; i++)
		stored(810400 + i, SPARKIE_CARAPACE_FRAGMENT, 1);
	EXPECT_EQ(ItemService::addItem(player(), TRAINING_SWORD, 3), 1) << "two slots were free";
	std::vector<Ptr<Item>> created = newItemsOf(TRAINING_SWORD);
	ASSERT_EQ(created.size(), 2u);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 5u);
	EXPECT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_INVENTORY_ADD_ITEM_OPCODE, SM_CUBE_UPDATE_OPCODE, SM_INVENTORY_ADD_ITEM_OPCODE,
									  SM_CUBE_UPDATE_OPCODE, SM_SYSTEM_MESSAGE_OPCODE}));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 26));
	EXPECT_EQ(packets[3], cubeSize(StorageType::CUBE, 27));
	EXPECT_EQ(packets[4], serialized(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR()));
	for (const Ptr<Item>& sword : created)
		EXPECT_EQ(sword->getItemCount(), 1);
}

TEST_F(ItemServiceTest, AddingASourceItemCopiesItsStonesEnchantmentAndBinding) {
	// ItemService.java:49-51 -> addNonStackableItem -> copyItemInfo (:123-143): optional sockets, creator, the godstone with its activation
	// count, enchant level, amplification, buff skill, tempering, soul binding, tune count, color, enchant bonus and skin; the source's count
	// with allowInventoryOverflow = true
	Ref<Item> source = Item::create(810500, dataholders::DataManager::ITEM_DATA->getItemTemplate(TRAINING_SWORD));
	items.push_back(source);
	source->setOptionalSockets(1);
	source->setItemCreator("Smith");
	source->addGodStone(FX_TEST_EARTH_GODSTONE, 2);
	source->setEnchantLevel(3);
	source->setAmplified(true);
	source->setBuffSkill(7);
	source->setTempering(4);
	source->setSoulBound(true);
	source->setTuneCount(2);
	source->setItemColor(0xFF00FF);
	source->setEnchantBonus(1);
	source->setItemSkinTemplate(dataholders::DataManager::ITEM_DATA->getItemTemplate(TAHABATA_SWORD));

	EXPECT_EQ(ItemService::addItem(player(), *source), 0);
	std::vector<Ptr<Item>> created = newItemsOf(TRAINING_SWORD);
	ASSERT_EQ(created.size(), 1u);
	Item& copy = *created[0];
	EXPECT_NE(copy.getObjectId(), source->getObjectId()) << "a new item";
	EXPECT_EQ(copy.getOptionalSockets(), 1);
	EXPECT_EQ(copy.getItemCreator(), "Smith");
	ASSERT_TRUE(copy.getGodStone());
	EXPECT_EQ(copy.getGodStone()->getItemId(), FX_TEST_EARTH_GODSTONE);
	EXPECT_EQ(copy.getGodStone()->getActivatedCount(), 2);
	EXPECT_NE(copy.getGodStone().get(), source->getGodStone().get()) << "Item.addGodStone makes the copy's own stone";
	EXPECT_EQ(copy.getEnchantLevel(), 3);
	EXPECT_TRUE(copy.isAmplified());
	EXPECT_EQ(copy.getBuffSkill(), 7);
	EXPECT_EQ(copy.getTempering(), 4);
	EXPECT_TRUE(copy.isSoulBound());
	EXPECT_EQ(copy.getTuneCount(), 2);
	EXPECT_EQ(copy.getItemColor(), std::optional<int32_t>(0xFF00FF));
	EXPECT_EQ(copy.getEnchantBonus(), 1);
	EXPECT_EQ(copy.getItemSkinTemplate(), dataholders::DataManager::ITEM_DATA->getItemTemplate(TAHABATA_SWORD));
	EXPECT_FALSE(copy.getIdianStone()) << "the source has none";

	// ItemService.java:56-58: addItem(player, sourceItem, count) takes the count and does not allow the overflow; a full cube keeps it
	for (int32_t i = 0; i < 26; i++)
		stored(810510 + i, SPARKIE_CARAPACE_FRAGMENT, 1);
	ASSERT_TRUE(player().getInventory().isFull());
	clearSent();
	EXPECT_EQ(ItemService::addItem(player(), *source, 2), 2);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR())}));

	// :49-51: addItem(player, sourceItem) passes allowInventoryOverflow = true, and addNonStackableItem's loop runs while
	// `allowInventoryOverflow || !inventory.isFull(...)` (:106): the full cube takes the copy, no STR_MSG_DICE_INVEN_ERROR
	const int32_t firstCopyId = copy.getObjectId();
	clearSent();
	EXPECT_EQ(ItemService::addItem(player(), *source), 0);
	EXPECT_EQ(player().getInventory().size(), 28);
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_INVENTORY_ADD_ITEM_OPCODE, SM_CUBE_UPDATE_OPCODE}));
	std::vector<Ptr<Item>> overflow = newItemsOf(TRAINING_SWORD, {firstCopyId});
	ASSERT_EQ(overflow.size(), 1u);
	ASSERT_EQ(packets.size(), 2u);
	expectAddItem(packets[0], *overflow[0], 0x19);
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 28));
}

TEST_F(ItemServiceTest, AddingASourceItemCopiesItsBonusStats) {
	// copyItemInfo (ItemService.java:138): newItem.setBonusStats(sourceItem.getBonusStatsId(), true) - Item.setBonusStats (Item.java:819-827)
	// makes a RandomBonusEffect(INVENTORY, the template's rnd_bonus, id). "Tune, Retune_Test_Option" has rnd_bonus 1 (item_templates.xml:9614),
	// whose INVENTORY set 1 has two modifier groups (item_random_bonuses.xml:3); the source rolled the second
	xml::LoadContext context;
	dataholders::DataManager::ITEM_RANDOM_BONUSES.publish(xml::bindString<dataholders::ItemRandomBonusData>(context, ITEM_RANDOM_BONUSES_XML));
	struct BonusesReset {
		~BonusesReset() { dataholders::DataManager::ITEM_RANDOM_BONUSES.resetForTests(); }
	} bonusesReset;
	Ref<Item> source = Item::create(810550, dataholders::DataManager::ITEM_DATA->getItemTemplate(TUNE_RETUNE_TEST_OPTION));
	items.push_back(source);
	source->setBonusStats(2, false);
	ASSERT_EQ(source->getBonusStatsId(), 2);

	EXPECT_EQ(ItemService::addItem(player(), *source), 0);
	std::vector<Ptr<Item>> created = newItemsOf(TUNE_RETUNE_TEST_OPTION);
	ASSERT_EQ(created.size(), 1u);
	EXPECT_EQ(created[0]->getBonusStatsId(), 2);
}

// ------------------------------------------------------------------------------------------------------------------------- ItemUpdatePredicate

TEST_F(ItemServiceTest, ThePredicatesUpdateTypeOfKinahFollowsItsAddType) {
	// ItemUpdatePredicate.getUpdateType (ItemService.java:194-198) -> ItemUpdateType.getKinahUpdateTypeFromAddType (ItemPacketService.java:71-80)
	Item& kinah = loose(810600, KINAH, 10);
	Item& potion = loose(810601, MINOR_LIFE_POTION, 10);
	Ref<ItemService::ItemUpdatePredicate> collect = ItemService::ItemUpdatePredicate::create();
	EXPECT_EQ(collect->getAddType(), ItemAddType::ITEM_COLLECT) << "the no-argument constructor (:190-192)";
	EXPECT_EQ(collect->getUpdateType(kinah, true), ItemUpdateType::INC_KINAH_COLLECT);
	EXPECT_EQ(collect->getUpdateType(kinah, false), ItemUpdateType::DEC_KINAH_BUY);
	EXPECT_EQ(collect->getUpdateType(potion, true), ItemUpdateType::INC_ITEM_COLLECT);

	Ref<ItemService::ItemUpdatePredicate> buy = ItemService::ItemUpdatePredicate::create(ItemAddType::BUY, ItemUpdateType::INC_ITEM_BUY);
	EXPECT_EQ(buy->getUpdateType(kinah, true), ItemUpdateType::INC_KINAH_SELL);
	EXPECT_EQ(buy->getUpdateType(potion, true), ItemUpdateType::INC_ITEM_BUY) << "not kinah: the predicate's own update type";
	EXPECT_EQ(buy->getUpdateType(potion, false), ItemUpdateType::INC_ITEM_BUY);

	Ref<ItemService::ItemUpdatePredicate> quest =
		ItemService::ItemUpdatePredicate::create(ItemAddType::QUEST_WORK_ITEM, ItemUpdateType::INC_ITEM_COLLECT);
	EXPECT_EQ(quest->getUpdateType(kinah, true), ItemUpdateType::INC_KINAH_QUEST);
	Ref<ItemService::ItemUpdatePredicate> mail = ItemService::ItemUpdatePredicate::create(ItemAddType::MAIL, ItemUpdateType::INC_ITEM_COLLECT);
	EXPECT_EQ(mail->getUpdateType(kinah, true), ItemUpdateType::INC_KINAH_MERGE) << "default arm";
	EXPECT_EQ(mail->getUpdateType(kinah, false), ItemUpdateType::DEC_KINAH_BUY);
}

TEST_F(ItemServiceTest, APredicatesAddAndUpdateTypesReachTheStorage) {
	// ItemService.java:42-44 with an ItemUpdatePredicate(BUY, INC_ITEM_BUY): a merge sends INC_ITEM_BUY (0x1C), a new stack is added with BUY
	// (0x1C); the predicate's changeItem is not called for stackable items (only addNonStackableItem calls it, :113)
	Item& potions = stored(810610, MINOR_LIFE_POTION, 10);
	Ref<ItemService::ItemUpdatePredicate> buy = ItemService::ItemUpdatePredicate::create(ItemAddType::BUY, ItemUpdateType::INC_ITEM_BUY);
	EXPECT_EQ(ItemService::addItem(player(), MINOR_LIFE_POTION, 2, false, *buy), 0);
	EXPECT_EQ(ItemService::addItem(player(), BANDAGE, 2, false, *buy), 0);
	std::vector<Ptr<Item>> bandages = newItemsOf(BANDAGE);
	ASSERT_EQ(bandages.size(), 1u);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 3u);
	expectUpdateItem(packets[0], potions, 0x1C);
	expectAddItem(packets[1], *bandages[0], 0x1C);
	EXPECT_EQ(packets[2], cubeSize(StorageType::CUBE, 2));
}

// ------------------------------------------------------------------------------------------------------------------------- enum companions

TEST(ItemPacketServiceEnumCompanionTest, UpdateTypeMasksAndSendabilityAreJavasConstructorArguments) {
	// ItemPacketService.java:28-53, the constants in ordinal order
	const std::vector<std::pair<int32_t, bool>> java{{-1, false}, {-2, false}, {-3, false}, {0, true}, {0x01, true}, {0x05, true}, {0x06, true},
		{0x0A, true}, {0x13, true}, {0x16, true}, {0x17, true}, {0x19, true}, {0x1A, true}, {0x1C, true}, {0x1D, true}, {0x20, true}, {0x23, true},
		{0x25, true}, {0x32, true}, {0x49, true}, {0x4B, true}, {0x50, true}, {0x51, true}, {0x5A, true}, {0x5E, true}, {0x8A, true}};
	ASSERT_EQ(java.size(), xml::EnumTraits<ItemUpdateType>::names.size());
	for (size_t i = 0; i < java.size(); i++) {
		SCOPED_TRACE(std::string(xml::EnumTraits<ItemUpdateType>::names[i]));
		EXPECT_EQ(getMask(static_cast<ItemUpdateType>(i)), java[i].first);
		EXPECT_EQ(isSendable(static_cast<ItemUpdateType>(i)), java[i].second);
	}
}

TEST(ItemPacketServiceEnumCompanionTest, AddAndDeleteTypeMasksAreJavasConstructorArguments) {
	// ItemPacketService.java:84-101 (ItemAddType) and :115-125 (ItemDeleteType)
	const std::vector<int32_t> addMasks{0x00, 0x07, 0x13, 0x19, 0x1C, 0x21, 0x23, 0x2B, 0x2D, 0x2E, 0x2F, 0x30, 0x35, 0x36, 0x36, 0x40, 0x50, 0x51};
	ASSERT_EQ(addMasks.size(), xml::EnumTraits<ItemAddType>::names.size());
	for (size_t i = 0; i < addMasks.size(); i++)
		EXPECT_EQ(getMask(static_cast<ItemAddType>(i)), addMasks[i]) << xml::EnumTraits<ItemAddType>::names[i];
	const std::vector<int32_t> deleteMasks{0x00, 0x04, 0x14, 0x15, 0x17, 0x1F, 0x31, 0x34, 0x66, 0x78, 0x26};
	ASSERT_EQ(deleteMasks.size(), xml::EnumTraits<ItemDeleteType>::names.size());
	for (size_t i = 0; i < deleteMasks.size(); i++)
		EXPECT_EQ(getMask(static_cast<ItemDeleteType>(i)), deleteMasks[i]) << xml::EnumTraits<ItemDeleteType>::names[i];
}

TEST(ItemPacketServiceEnumCompanionTest, TheStaticMethodsMapLikeJavasSwitches) {
	// ItemDeleteType.fromUpdateType (ItemPacketService.java:137-144): three update types have their own delete type, all others DEFAULT
	for (size_t i = 0; i < xml::EnumTraits<ItemUpdateType>::names.size(); i++) {
		ItemUpdateType updateType = static_cast<ItemUpdateType>(i);
		ItemDeleteType expected = updateType == ItemUpdateType::DEC_ITEM_SPLIT ? ItemDeleteType::SPLIT
			: updateType == ItemUpdateType::DEC_ITEM_USE						? ItemDeleteType::USE
			: updateType == ItemUpdateType::DEC_ITEM_SPLIT_MOVE				? ItemDeleteType::MOVE
																				: ItemDeleteType::DEFAULT;
		EXPECT_EQ(fromUpdateType(updateType), expected) << xml::EnumTraits<ItemUpdateType>::names[i];
	}
	// ItemDeleteType.fromQuestStatus (:146-152)
	EXPECT_EQ(fromQuestStatus(questEngine::model::QuestStatus::START), ItemDeleteType::QUEST_START);
	EXPECT_EQ(fromQuestStatus(questEngine::model::QuestStatus::COMPLETE), ItemDeleteType::QUEST_COMPLETE);
	EXPECT_EQ(fromQuestStatus(questEngine::model::QuestStatus::REWARD), ItemDeleteType::DEFAULT);
	EXPECT_EQ(fromQuestStatus(questEngine::model::QuestStatus::LOCKED), ItemDeleteType::DEFAULT);
	// ItemUpdateType.getKinahUpdateTypeFromAddType (:71-80): every add type, both directions
	for (size_t i = 0; i < xml::EnumTraits<ItemAddType>::names.size(); i++) {
		ItemAddType addType = static_cast<ItemAddType>(i);
		ItemUpdateType increase = addType == ItemAddType::BUY ? ItemUpdateType::INC_KINAH_SELL
			: addType == ItemAddType::ITEM_COLLECT			  ? ItemUpdateType::INC_KINAH_COLLECT
			: addType == ItemAddType::QUEST_WORK_ITEM		  ? ItemUpdateType::INC_KINAH_QUEST
															  : ItemUpdateType::INC_KINAH_MERGE;
		EXPECT_EQ(getKinahUpdateTypeFromAddType(addType, true), increase) << xml::EnumTraits<ItemAddType>::names[i];
		EXPECT_EQ(getKinahUpdateTypeFromAddType(addType, false), ItemUpdateType::DEC_KINAH_BUY) << xml::EnumTraits<ItemAddType>::names[i];
	}
}

} // namespace
} // namespace aion::gameserver::services::item::test
