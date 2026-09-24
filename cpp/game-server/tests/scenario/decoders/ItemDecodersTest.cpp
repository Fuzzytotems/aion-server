// The G-02 item and loot decoders (m5b3-plan.md §2.7, G-02) against byte vectors written from the Java writeImpl, in the style of
// CombatDecodersTest.cpp: a hand-built body with the offset of every field where the layout is flat, builder cases written out field by field
// in Java order where a packet carries an item info blob, and every case asserts the body size Java produces. The item rows are shipped data
// (item_templates.xml line cited at each), their l10n strings ChatUtil.l10n of the template's desc. No case includes or consults a C++
// serverpackets header (m5a-plan.md D9).

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "decoders/ItemDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/**
 * ChatUtil.l10n(desc) (ChatUtil.java:95-101), written as the UTF-16 code units writeS puts on the wire plus its NUL: "$", then the two chars of
 * `desc << 1 | 1`, low half first. Returns the UTF-8 the decoder hands back.
 */
std::string l10n(PacketWriter& writer, int32_t desc) {
	const uint32_t id = static_cast<uint32_t>(desc) << 1 | 1;
	const std::u16string chars{u'$', static_cast<char16_t>(id & 0xFFFF), static_cast<char16_t>(id >> 16)};
	for (const char16_t c : chars)
		writer.H(c);
	writer.H(0);
	return commons::utils::StringUtils::toUtf8(chars);
}

/** GeneralInfoBlobEntry (0x00): the mask, the count, an empty creator and the constants PacketDecoders.cpp verifies */
void generalInfoEntry(PacketWriter& w, uint16_t itemMask, int64_t count) {
	w.C(0x00);
	w.H(itemMask).Q(count).S("").C(0).D(0).D(0).D(0).H(0).D(0).H(18);
}

/** EnchantInfoBlobEntry (0x0B) of an item without enchantment, with `godStoneId` in its god stone field (EnchantInfoBlobEntry.java:51) */
void enchantInfoEntry(PacketWriter& w, int32_t skinTemplateId, int32_t godStoneId) {
	w.C(0x0B);
	w.C(0).C(0).D(skinTemplateId).C(0).C(0);
	for (int i = 0; i < 6; i++) // Item.MAX_BASIC_STONES manastones
		w.D(0);
	w.D(godStoneId);
	w.zeros(4);       // dye info of a null color
	w.C(0).D(0).D(0); // unknown, 1.5.1.9, dye expiration
	w.D(0).C(0).C(0); // idian stone, polish number, tempering
	w.zeros(18).zeros(16).zeros(16).zeros(16);
	w.D(0).C(0).D(0).D(0).D(0); // 4.7.5, amplified, buff skill, the two unused skill ids
}

/** ItemInfoBlob.writeMe: the entries prefixed with their size */
void blob(PacketWriter& w, const PacketWriter& entries) {
	w.H(static_cast<int32_t>(entries.data.size())).B(entries.data);
}

/** getFullBlob of an item whose group has no equipment slot (a potion, a junk item, kinah): GENERAL_INFO alone */
PacketWriter plainBlob(uint16_t itemMask, int64_t count) {
	PacketWriter entries;
	generalInfoEntry(entries, itemMask, count);
	return entries;
}

/**
 * getFullBlob of the Training Sword, item_templates.xml:375 (id 100000094, SWORD, mask 138366): EQUIPPED_SLOT, SLOTS_WEAPON, ENCHANT_INFO,
 * PREMIUM_OPTION, GENERAL_INFO in ItemInfoBlob.getFullBlob's order (ItemInfoBlob.java getFullBlob)
 */
PacketWriter swordBlob(int64_t equippedSlot, int32_t godStoneId) {
	PacketWriter entries;
	entries.C(0x06).Q(equippedSlot);
	entries.C(0x01).Q(1).Q(0); // SLOTS_WEAPON: MAIN_HAND and no secondary slot
	enchantInfoEntry(entries, 100000094, godStoneId);
	entries.C(0x10).C(0).C(0).C(0);
	generalInfoEntry(entries, static_cast<uint16_t>(138366 & 0xFFFF), 1);
	return entries;
}

// ---- SM_LOOT_ITEMLIST -------------------------------------------------------------------------------------------------------------------

TEST(ItemDecodersTest, LootItemListFromAHandBuiltBody) {
	// SM_LOOT_ITEMLIST.java:41-56: a 210663 corpse at the M5b-3 gate's rate lists (among its ten) index 4 = the Minor Power Shard,
	// item_templates.xml:848722 (count 2..15, no option slot), and index 9 = one Plainsman's Boots, :545809 (option_slot_bonus="1": socket -1)
	const std::vector<uint8_t> body = {
		0x2A, 0x00, 0x0B, 0x40, // 0: writeD(targetObjectId) = the corpse               (:41)
		0x02,                   // 4: writeC(dropItems.size())                           (:42)
		0x04,                   // 5: writeC(index)                                      (:46)
		0x43, 0xBC, 0x12, 0x0A, // 6: writeD(itemId) = 169000003                         (:47)
		0x07, 0x00, 0x00, 0x00, // 10: writeD((int) count) = 7                           (:48)
		0x00,                   // 14: writeC(optionalSocket) = 0                        (:49)
		0x00, 0x00,             // 15: writeC(0), writeC(0)                              (:50-51)
		0x00,                   // 17: writeC(showLootConfirmation) = 0, a solo looter   (:56)
		0x09,                   // 18: index 9
		0x9D, 0x15, 0xD0, 0x06, // 19: itemId = 114300317
		0x01, 0x00, 0x00, 0x00, // 23: count 1
		0xFF,                   // 27: optionalSocket = -1
		0x00, 0x00,             // 28
		0x00,                   // 30
	};
	ASSERT_EQ(body.size(), 31u);

	const LootItemList list = decodeLootItemList(body);
	EXPECT_EQ(list.targetObjectId, 0x400B002A);
	ASSERT_EQ(list.items.size(), 2u);
	EXPECT_EQ(list.items[0], (LootItem{4, 169000003, 7, 0, false}));
	EXPECT_EQ(list.items[1], (LootItem{9, 114300317, 1, -1, false})) << "the socket byte is signed: -1 for an option slot bonus";

	PacketWriter empty; // resendDropList's shorter list once everything but nothing is left is still a list of 0
	empty.D(0x400B002A).C(0);
	EXPECT_TRUE(decodeLootItemList(empty.data).items.empty());

	std::vector<uint8_t> changedConstant = body;
	changedConstant[15] = 1;
	EXPECT_THROW(decodeLootItemList(changedConstant), DecodeError) << "writeC(0) at :50 is a constant";
	std::vector<uint8_t> notAFlag = body;
	notAFlag[17] = 2;
	EXPECT_THROW(decodeLootItemList(notAFlag), DecodeError) << "showLootConfirmation is `? 1 : 0`";
	std::vector<uint8_t> countTooHigh = body;
	countTooHigh[4] = 3;
	EXPECT_THROW(decodeLootItemList(countTooHigh), DecodeError) << "a third entry that is not there";
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeLootItemList(trailing), DecodeError);
}

// ---- SM_INVENTORY_ADD_ITEM --------------------------------------------------------------------------------------------------------------

TEST(ItemDecodersTest, InventoryAddItemOfALootedJunkStack) {
	// a new stack from a loot (ItemService.addStackableItem -> Storage.add -> ItemPacketService.java:217, ItemAddType.ITEM_COLLECT): the Sparkie
	// Carapace Fragment, item_templates.xml:874138 (mask 12414, desc 718718), at cube slot 11
	PacketWriter w;
	w.H(ITEM_ADD_ITEM_COLLECT).H(1); // SM_INVENTORY_ADD_ITEM.java:38-39
	w.D(0x7000001).D(182004793);     // :47-48
	const std::string name = l10n(w, 718718); // :49
	const size_t blobAt = w.data.size();
	blob(w, plainBlob(12414, 1));     // :51
	const size_t blobSize = w.data.size() - blobAt;
	w.H(11).C(0);                     // :53-54
	ASSERT_EQ(w.data.size(), 4u + 8u + 8u + blobSize + 3u);

	const InventoryAddItem add = decodeInventoryAddItem(w.data);
	EXPECT_EQ(add.addTypeMask, ITEM_ADD_ITEM_COLLECT);
	ASSERT_EQ(add.items.size(), 1u);
	const InventoryItem& item = add.items[0];
	EXPECT_EQ(item.objectId, 0x7000001);
	EXPECT_EQ(item.templateId, 182004793);
	EXPECT_EQ(item.l10n, name) << "ChatUtil.l10n: '$' and the two chars of desc * 2 + 1";
	EXPECT_EQ(item.blobEntryIds, (std::vector<uint8_t>{0x00}));
	ASSERT_TRUE(item.general);
	EXPECT_EQ(item.general->count, 1);
	EXPECT_EQ(item.general->itemMask, 12414);
	EXPECT_EQ(item.equipmentSlot, 11);
	EXPECT_FALSE(item.cloth);

	PacketWriter twoItems; // the list form (a buy): each item is complete, the count comes first
	twoItems.H(ITEM_ADD_BUY).H(2);
	for (const int32_t objectId : {0x7000002, 0x7000003}) {
		twoItems.D(objectId).D(162000002);
		l10n(twoItems, 702583); // item_templates.xml:830724, Minor Life Potion
		blob(twoItems, plainBlob(12414, 10));
		twoItems.H(0xFFFF).C(0);
	}
	const InventoryAddItem two = decodeInventoryAddItem(twoItems.data);
	ASSERT_EQ(two.items.size(), 2u);
	EXPECT_EQ(two.items[1].objectId, 0x7000003);
	EXPECT_EQ(two.items[1].equipmentSlot, 0xFFFF) << "writeH(slot & 0xFFFF) of ItemStorage.FIRST_AVAILABLE_SLOT";

	std::vector<uint8_t> clothNotAFlag = w.data;
	clothNotAFlag.back() = 2;
	EXPECT_THROW(decodeInventoryAddItem(clothNotAFlag), DecodeError);
	std::vector<uint8_t> missingCloth = w.data;
	missingCloth.pop_back();
	EXPECT_THROW(decodeInventoryAddItem(missingCloth), DecodeError) << "the cube's item ends with the cloth byte (:54)";
}

// ---- SM_INVENTORY_UPDATE_ITEM -----------------------------------------------------------------------------------------------------------

TEST(ItemDecodersTest, InventoryUpdateItemOfAMergeCarriesTheSendableMask) {
	// a merge into an existing stack (Storage.increaseItemCount -> ItemPacketService.java:194): object id, l10n, the full blob and the update
	// type - no template id and no slot (SM_INVENTORY_UPDATE_ITEM.java:35-59)
	PacketWriter w;
	w.D(0x7000004);
	const std::string name = l10n(w, 718718);
	blob(w, plainBlob(12414, 3));
	w.H(ITEM_UPDATE_INC_ITEM_COLLECT);

	const InventoryUpdateItem update = decodeInventoryUpdateItem(w.data);
	EXPECT_EQ(update.item.objectId, 0x7000004);
	EXPECT_EQ(update.item.l10n, name);
	EXPECT_EQ(update.item.templateId, 0) << "SM_INVENTORY_UPDATE_ITEM does not write the template id";
	ASSERT_TRUE(update.item.general);
	EXPECT_EQ(update.item.general->count, 3) << "the count after the merge";
	ASSERT_TRUE(update.updateTypeMask);
	EXPECT_EQ(*update.updateTypeMask, ITEM_UPDATE_INC_ITEM_COLLECT);

	std::vector<uint8_t> oneByte = w.data;
	oneByte.pop_back();
	EXPECT_THROW(decodeInventoryUpdateItem(oneByte), DecodeError) << "half an update type short";
	std::vector<uint8_t> threeBytes = w.data;
	threeBytes.push_back(0);
	EXPECT_THROW(decodeInventoryUpdateItem(threeBytes), DecodeError) << "more than the update type after the blob";
}

TEST(ItemDecodersTest, InventoryUpdateItemOfAnEquipChangeHasOnlyTheSlotEntryAndNoMask) {
	// ItemPacketService.updateItemAfterEquip (:164): EQUIP_UNEQUIP builds a blob with EQUIPPED_SLOT alone (SM_INVENTORY_UPDATE_ITEM.java:40-43)
	// and is not sendable (-1, ItemPacketService.java:28), so no mask follows. The Training Sword leaves the main hand: its slot entry is 0
	PacketWriter entries;
	entries.C(0x06).Q(0);
	PacketWriter w;
	w.D(0x7000005);
	l10n(w, 700775); // item_templates.xml:375
	blob(w, entries);
	ASSERT_EQ(w.data.size(), 4u + 8u + 2u + 9u);

	const InventoryUpdateItem update = decodeInventoryUpdateItem(w.data);
	EXPECT_EQ(update.item.blobEntryIds, (std::vector<uint8_t>{0x06}));
	ASSERT_TRUE(update.item.equippedSlotBlob);
	EXPECT_EQ(*update.item.equippedSlotBlob, 0);
	EXPECT_FALSE(update.item.general) << "only the slot entry";
	EXPECT_FALSE(update.updateTypeMask) << "EQUIP_UNEQUIP is internal";
}

TEST(ItemDecodersTest, InventoryUpdateItemCarriesASocketedGodstoneInTheEnchantEntry) {
	// Y6: after socketGodstone, updateItemAfterInfoChange (ItemPacketService.java:155-157, DEC_ITEM_USE by the one-argument constructor)
	// sends the sword's full blob, and the godstone is ENCHANT_INFO's god stone field - 168000116 "Fx Test Earth Godstone",
	// item_templates.xml:848006. This is m5b3-plan.md §13 question 2: the existing blob reader needs no new entry for it
	PacketWriter w;
	w.D(0x7000006);
	l10n(w, 700775);
	blob(w, swordBlob(0, 168000116));
	w.H(ITEM_UPDATE_DEC_ITEM_USE);

	const InventoryUpdateItem update = decodeInventoryUpdateItem(w.data);
	EXPECT_EQ(update.item.blobEntryIds, (std::vector<uint8_t>{0x06, 0x01, 0x0B, 0x10, 0x00}));
	ASSERT_TRUE(update.item.enchant);
	EXPECT_EQ(update.item.enchant->godStoneId, 168000116);
	EXPECT_EQ(update.item.enchant->skinTemplateId, 100000094);
	ASSERT_TRUE(update.updateTypeMask);
	EXPECT_EQ(*update.updateTypeMask, ITEM_UPDATE_DEC_ITEM_USE);
}

// ---- SM_DELETE_ITEM, SM_CUBE_UPDATE -------------------------------------------------------------------------------------------------------

TEST(ItemDecodersTest, DeleteItemFromAHandBuiltBody) {
	const std::vector<uint8_t> body = {
		0x08, 0x00, 0x00, 0x07, // 0: writeD(itemObjectId)                       (SM_DELETE_ITEM.java:25)
		0x15,                   // 4: writeC(ItemDeleteType.DISCARD.getMask())   (:26)
	};
	const DeleteItem deleted = decodeDeleteItem(body);
	EXPECT_EQ(deleted.objectId, 0x07000008);
	EXPECT_EQ(deleted.deleteTypeMask, ITEM_DELETE_DISCARD);
	std::vector<uint8_t> shortInt = {0x08, 0x00, 0x00, 0x07};
	EXPECT_THROW(decodeDeleteItem(shortInt), DecodeError) << "the delete type byte is missing";
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeDeleteItem(trailing), DecodeError);
}

TEST(ItemDecodersTest, CubeUpdateSizeArmAndStigmaArm) {
	const std::vector<uint8_t> cube = {
		0x00,                   // 0: writeC(action) = 0, cubeSize          (SM_CUBE_UPDATE.java:70)
		0x01,                   // 1: writeC(actionValue) = REGULAR_WAREHOUSE.ordinal()   (:71)
		0x0C, 0x00, 0x00, 0x00, // 2: writeD(itemsCount) = 12               (:74)
		0x02, 0x01, 0x00,       // 6: npcExpands, questExpands, itemExpands (:75-77)
	};
	const CubeUpdate size = decodeCubeUpdate(cube);
	EXPECT_EQ(size.action, 0);
	EXPECT_EQ(size.actionValue, STORAGE_REGULAR_WAREHOUSE);
	EXPECT_EQ(size.itemsCount, 12);
	EXPECT_EQ(size.npcExpands, 2);
	EXPECT_EQ(size.questExpands, 1);
	EXPECT_EQ(size.itemExpands, 0);

	const std::vector<uint8_t> stigma = {0x06, 0x03}; // stigmaSlots(3): action 6 writes nothing more (:72-79)
	const CubeUpdate slots = decodeCubeUpdate(stigma);
	EXPECT_EQ(slots.action, 6);
	EXPECT_EQ(slots.actionValue, 3);
	EXPECT_EQ(slots.itemsCount, 0);

	std::vector<uint8_t> truncated = cube;
	truncated.pop_back();
	EXPECT_THROW(decodeCubeUpdate(truncated), DecodeError);
	std::vector<uint8_t> stigmaWithSize = cube;
	stigmaWithSize[0] = 6;
	EXPECT_THROW(decodeCubeUpdate(stigmaWithSize), DecodeError) << "only action 0 carries the size fields";
}

// ---- the warehouse packets ----------------------------------------------------------------------------------------------------------------

TEST(ItemDecodersTest, WarehouseAddItemHasTheLiteralByteAndNoClothFlag) {
	// Y9: a junk stack moved into the regular warehouse (ItemPacketService.java:225): warehouse type 1, the add type, one item with a literal
	// writeC(0) between its template id and its l10n, and no cloth byte after its slot (SM_WAREHOUSE_ADD_ITEM.java:32-52)
	PacketWriter w;
	w.C(STORAGE_REGULAR_WAREHOUSE).H(ITEM_ADD_ALL_SLOT).H(1);
	w.D(0x7000009).D(182004793).C(0);
	const std::string name = l10n(w, 718718);
	blob(w, plainBlob(12414, 4));
	w.H(0);

	const WarehouseAddItem add = decodeWarehouseAddItem(w.data);
	EXPECT_EQ(add.warehouseType, STORAGE_REGULAR_WAREHOUSE);
	EXPECT_EQ(add.addTypeMask, ITEM_ADD_ALL_SLOT);
	ASSERT_EQ(add.items.size(), 1u);
	EXPECT_EQ(add.items[0].objectId, 0x7000009);
	EXPECT_EQ(add.items[0].templateId, 182004793);
	EXPECT_EQ(add.items[0].l10n, name);
	ASSERT_TRUE(add.items[0].general);
	EXPECT_EQ(add.items[0].general->count, 4);
	EXPECT_EQ(add.items[0].equipmentSlot, 0);

	PacketWriter literal;
	literal.C(STORAGE_REGULAR_WAREHOUSE).H(ITEM_ADD_ALL_SLOT).H(1);
	literal.D(0x7000009).D(182004793).C(4); // "4 - weapon": Java always writes 0
	l10n(literal, 718718);
	blob(literal, plainBlob(12414, 4));
	literal.H(0);
	EXPECT_THROW(decodeWarehouseAddItem(literal.data), DecodeError) << "the writeC(0) at :46 is a constant";
	std::vector<uint8_t> withCloth = w.data;
	withCloth.push_back(0);
	EXPECT_THROW(decodeWarehouseAddItem(withCloth), DecodeError) << "a warehouse item has no cloth byte";
}

TEST(ItemDecodersTest, WarehouseUpdateItemAndDeleteWarehouseItem) {
	// a merge into a warehouse stack (ItemPacketService.java:202): the object id BEFORE the warehouse type, a GENERAL_INFO-only blob
	// (SM_WAREHOUSE_UPDATE_ITEM.java:33-42)
	PacketWriter w;
	w.D(0x700000A).C(STORAGE_REGULAR_WAREHOUSE);
	l10n(w, 718718);
	blob(w, plainBlob(12414, 9));
	w.H(ITEM_UPDATE_INC_ITEM_MERGE);
	const WarehouseUpdateItem update = decodeWarehouseUpdateItem(w.data);
	EXPECT_EQ(update.item.objectId, 0x700000A);
	EXPECT_EQ(update.warehouseType, STORAGE_REGULAR_WAREHOUSE);
	ASSERT_TRUE(update.item.general);
	EXPECT_EQ(update.item.general->count, 9);
	ASSERT_TRUE(update.updateTypeMask);
	EXPECT_EQ(*update.updateTypeMask, ITEM_UPDATE_INC_ITEM_MERGE);

	// Y16: the swap deletes the warehouse item first (ItemPacketService.java:182)
	const std::vector<uint8_t> body = {
		0x01,                   // 0: writeC(warehouseType) = REGULAR_WAREHOUSE.getId()   (SM_DELETE_WAREHOUSE_ITEM.java:24)
		0x0B, 0x00, 0x00, 0x07, // 1: writeD(itemObjId)                                  (:25)
		0x14,                   // 5: writeC(ItemDeleteType.MOVE.getMask())              (:26)
	};
	const DeleteWarehouseItem deleted = decodeDeleteWarehouseItem(body);
	EXPECT_EQ(deleted.warehouseType, STORAGE_REGULAR_WAREHOUSE);
	EXPECT_EQ(deleted.objectId, 0x0700000B);
	EXPECT_EQ(deleted.deleteTypeMask, ITEM_DELETE_MOVE);
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeDeleteWarehouseItem(trailing), DecodeError);
}

// ---- SM_UPDATE_PLAYER_APPEARANCE, SM_ITEM_USAGE_ANIMATION -----------------------------------------------------------------------------------

TEST(ItemDecodersTest, UpdatePlayerAppearanceAfterTheSwordLeftTheMainHand) {
	// Y5: the Warrior's starter gear without the sword - Training Hauberk (TORSO, 1 << 3, item_templates.xml:232833) and Training Chausses
	// (PANTS, 1 << 12, :483689), ItemSlot.java - in writeEquippedItems' order (AbstractPlayerInfoPacket.java:148-165)
	PacketWriter w;
	w.D(0x100001);         // SM_UPDATE_PLAYER_APPEARANCE.java:23
	w.D((1 << 3) | (1 << 12)); // :157, the ORed slot mask
	for (const int32_t skin : {110500003, 113500001})
		w.D(skin).D(0).C(0).C(0).C(0).C(0).H(0).H(0); // :159-163
	ASSERT_EQ(w.data.size(), 4u + 4u + 2u * 16u);

	const UpdatePlayerAppearance appearance = decodeUpdatePlayerAppearance(w.data);
	EXPECT_EQ(appearance.playerObjectId, 0x100001);
	EXPECT_EQ(appearance.equipment.mask, (1 << 3) | (1 << 12));
	ASSERT_EQ(appearance.equipment.items.size(), 2u) << "the entry count is the mask's bit count";
	EXPECT_EQ(appearance.equipment.items[0].skinTemplateId, 110500003);
	EXPECT_EQ(appearance.equipment.items[1].skinTemplateId, 113500001);

	PacketWriter withSword; // re-equipped: the main hand bit and a third entry
	withSword.D(0x100001).D(1 | (1 << 3) | (1 << 12));
	for (const int32_t skin : {100000094, 110500003, 113500001})
		withSword.D(skin).D(skin == 100000094 ? 168000116 : 0).C(0).C(0).C(0).C(0).H(0).H(0);
	const UpdatePlayerAppearance equipped = decodeUpdatePlayerAppearance(withSword.data);
	ASSERT_EQ(equipped.equipment.items.size(), 3u);
	EXPECT_EQ(equipped.equipment.items[0].godStoneId, 168000116) << "writeEquippedItems writes the god stone of every item (:160)";

	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeUpdatePlayerAppearance(trailing), DecodeError) << "one entry per mask bit, nothing after them";
}

TEST(ItemDecodersTest, ItemUsageAnimationFromAHandBuiltBody) {
	// Y6: socketGodstone's opening animation, a 2 s use (ItemSocketService.java:190-191): the six-argument constructor (time 2000, end 0,
	// unk3 = its `unk` 0; SM_ITEM_USAGE_ANIMATION.java:41-49), which leaves unk2 at the field's initializer 1
	const std::vector<uint8_t> body = {
		0x01, 0x00, 0x10, 0x00, // 0: writeD(playerObjId)                    (:76)
		0x01, 0x00, 0x10, 0x00, // 4: writeD(targetObjId) = the player       (:77)
		0x0C, 0x00, 0x00, 0x07, // 8: writeD(itemObjId) = the stone          (:79)
		0x74, 0x7A, 0x03, 0x0A, // 12: writeD(itemId) = 168000116            (:80)
		0xD0, 0x07, 0x00, 0x00, // 16: writeD(time) = 2000                   (:82)
		0x00,                   // 20: writeC(end) = 0                       (:83)
		0x00,                   // 21: writeC(unk)                           (:84)
		0x00,                   // 22: writeC(unk1)                          (:85)
		0x01,                   // 23: writeC(unk2) = 1, the field's initializer (:86, :19)
		0x00, 0x00, 0x00, 0x00, // 24: writeD(unk3)                          (:87)
	};
	ASSERT_EQ(body.size(), 28u);
	const ItemUsageAnimation animation = decodeItemUsageAnimation(body);
	EXPECT_EQ(animation, (ItemUsageAnimation{0x100001, 0x100001, 0x0700000C, 168000116, 2000, 0, 0, 0, 1, 0}));

	std::vector<uint8_t> shortBody = body;
	shortBody.pop_back();
	EXPECT_THROW(decodeItemUsageAnimation(shortBody), DecodeError);
	std::vector<uint8_t> longBody = body;
	longBody.push_back(0);
	EXPECT_THROW(decodeItemUsageAnimation(longBody), DecodeError);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
