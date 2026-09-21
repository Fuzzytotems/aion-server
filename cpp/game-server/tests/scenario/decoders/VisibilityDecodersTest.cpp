// The F-08 decoders the visibility and enter-world assertions of the gate need (m5a-plan.md §5.4 V10, §5.5 V1-V4, §5.6 M1/M2): SM_NPC_INFO,
// SM_GATHERABLE_INFO, SM_QUEST_LIST, SM_WAREHOUSE_INFO and SM_MACRO_LIST. The builders below spell the Java writeImpl sequences out once more,
// field by field; every case asserts the body size Java produces, so a decoder that reads a field with the wrong width or in the wrong order
// cannot pass both the value assertions and the exact-consumption check of D9.

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "decoders/PacketDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_NPC_INFO.writeImpl, field by field (gear == null: a single zero int instead of the mask and the entries) */
std::vector<uint8_t> npcInfoBytes(int32_t objectId, int32_t templateId, float x, float y, float z, uint8_t heading, uint8_t hpPercentage,
                                  uint8_t level, const std::vector<int32_t>& gear = {}, int32_t gearMask = 0) {
	PacketWriter w;
	w.F(x).F(y).F(z);
	w.D(objectId);
	w.D(templateId); // hp gauge and talk properties
	w.D(templateId); // visual appearance
	w.C(38);         // CreatureType.getId()
	w.H(65);         // state: normal
	w.C(heading);
	w.D(templateId + 1); // l10n id
	w.D(0);              // title id
	w.H(0).C(0).D(0);    // unk
	w.D(0);              // creator id
	w.S("");             // master name
	w.C(hpPercentage);
	w.D(1234); // max hp
	w.C(level);
	w.D(gearMask);
	for (int32_t itemId : gear)
		w.D(itemId).D(0).D(0).H(0).H(0);
	w.F(1.5f).F(2.5f); // bound radius and height
	w.F(6.0f);         // movement speed
	w.H(1500).H(1500); // attack speed, twice
	w.C(0);            // neither a flag npc nor a new spawn
	w.F(x).F(y).F(z);  // move target
	w.C(0);            // movement mask
	w.H(77);           // static id
	for (int i = 0; i < 8; i++)
		w.C(0);
	w.C(0);  // visual state
	w.H(1);  // npc object type
	w.C(0);  // unk
	w.D(0);  // target object id
	w.D(12); // town id
	w.D(0);  // 4.7.5
	return w.data;
}

std::vector<uint8_t> gatherableInfoBytes(int32_t objectId, int32_t templateId, float x, float y, float z, uint16_t stateFlag = 1) {
	PacketWriter w;
	w.F(x).F(y).F(z);
	w.D(objectId);
	w.D(55); // static id
	w.D(templateId);
	w.H(stateFlag);
	w.C(90); // heading
	w.D(templateId + 7);
	w.H(0).H(0).H(0);
	w.C(100);
	return w.data;
}

std::vector<uint8_t> questListBytes(const std::vector<QuestEntry>& quests) {
	PacketWriter w;
	w.H(1);
	w.H(static_cast<int32_t>((0x10000u - quests.size()) & 0xFFFFu));
	for (const QuestEntry& quest : quests)
		w.D(quest.questId).C(quest.status).D(quest.questVarsAndFlags).C(quest.completeCount);
	return w.data;
}

std::vector<uint8_t> macroListBytes(int32_t playerObjectId, bool clearList, const std::vector<MacroEntry>& macros) {
	PacketWriter w;
	w.D(playerObjectId);
	w.C(clearList ? 1 : 0);
	w.H(static_cast<int32_t>((0x10000u - macros.size()) & 0xFFFFu));
	for (const MacroEntry& macro : macros)
		w.C(macro.id).S(macro.xml);
	return w.data;
}

/** an empty SM_WAREHOUSE_INFO of one storage type (the 43 the enter world burst sends are all empty for a new character) */
std::vector<uint8_t> emptyWarehouseBytes(uint8_t warehouseType, bool firstPacket, uint8_t expandLevel) {
	PacketWriter w;
	w.C(warehouseType).C(firstPacket ? 1 : 0).C(expandLevel);
	w.H(0); // no item: writeH(0), not the writeC(1)/writeC(0) branch
	w.H(0); // item count
	return w.data;
}

TEST(VisibilityDecodersTest, NpcInfoWithoutOverriddenEquipment) {
	const std::vector<uint8_t> body = npcInfoBytes(0x30000011, 203049, 1204.29f, 1053.18f, 138.962f, 116, 100, 20);
	EXPECT_EQ(body.size(), 115u) << "SM_NPC_INFO of an npc without a master name and without gear";
	const NpcInfo npc = decodeNpcInfo(body);
	EXPECT_EQ(npc.objectId, 0x30000011);
	EXPECT_EQ(npc.templateId, 203049);
	EXPECT_FLOAT_EQ(npc.x, 1204.29f);
	EXPECT_FLOAT_EQ(npc.y, 1053.18f);
	EXPECT_FLOAT_EQ(npc.z, 138.962f);
	EXPECT_EQ(npc.heading, 116);
	EXPECT_EQ(npc.hpPercentage, 100);
	EXPECT_EQ(npc.level, 20);
	EXPECT_EQ(npc.maxHp, 1234);
	EXPECT_EQ(npc.staticId, 77);
	EXPECT_EQ(npc.townId, 12);
	EXPECT_EQ(npc.creatorId, 0);
	EXPECT_EQ(npc.masterName, "");
	EXPECT_TRUE(npc.equipmentItemIds.empty());
	EXPECT_FLOAT_EQ(npc.movementSpeed, 6.0f);
}

TEST(VisibilityDecodersTest, NpcInfoDerivesTheEquipmentEntryCountFromTheSlotMask) {
	// NpcEquippedGear.init puts exactly one item into each slot it marks, so the bit count of the mask is the entry count
	const std::vector<uint8_t> body = npcInfoBytes(7, 210115, 1.0f, 2.0f, 3.0f, 0, 100, 24, {100000094, 110500003}, 0x0001 | 0x0008);
	const NpcInfo npc = decodeNpcInfo(body);
	EXPECT_EQ(npc.equipmentMask, 0x0009);
	EXPECT_EQ(npc.equipmentItemIds, (std::vector<int32_t>{100000094, 110500003}));

	// one bit too few in the mask leaves 16 bytes unread
	std::vector<uint8_t> shortMask = body;
	shortMask[55] = 0x01; // the low byte of the mask int (0x09 -> 0x01: one bit, but two entries follow)
	EXPECT_THROW(decodeNpcInfo(shortMask), DecodeError);
}

TEST(VisibilityDecodersTest, NpcInfoRejectsABodyThatIsNotTheJavaLayout) {
	const std::vector<uint8_t> body = npcInfoBytes(7, 203049, 1.0f, 2.0f, 3.0f, 0, 100, 20);
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeNpcInfo(trailing), DecodeError) << "a trailing byte must fail the exact consumption check";
	std::vector<uint8_t> truncated(body.begin(), body.end() - 1);
	EXPECT_THROW(decodeNpcInfo(truncated), DecodeError);
	// the two template ids must be equal: the decoder catches a port that writes the visual id from another field
	std::vector<uint8_t> differentIds = body;
	differentIds[20] = static_cast<uint8_t>(differentIds[20] + 1);
	EXPECT_THROW(decodeNpcInfo(differentIds), DecodeError);
	// the trailing unknown byte run is a constant of the packet
	std::vector<uint8_t> changedFiller = body;
	changedFiller[body.size() - 20] = 1;
	EXPECT_THROW(decodeNpcInfo(changedFiller), DecodeError);
}

TEST(VisibilityDecodersTest, GatherableInfo) {
	const std::vector<uint8_t> body = gatherableInfoBytes(0x40000001, 400030, 1200.5f, 1050.25f, 140.0f);
	EXPECT_EQ(body.size(), 38u);
	const GatherableInfo info = decodeGatherableInfo(body);
	EXPECT_EQ(info.objectId, 0x40000001);
	EXPECT_EQ(info.templateId, 400030);
	EXPECT_EQ(info.staticId, 55);
	EXPECT_EQ(info.stateFlag, 1);
	EXPECT_EQ(info.heading, 90);
	EXPECT_EQ(info.l10nId, 400037);
	EXPECT_FLOAT_EQ(info.x, 1200.5f);

	// a static door writes 9 (open) or 10 (closed) instead of 1
	EXPECT_EQ(decodeGatherableInfo(gatherableInfoBytes(2, 3, 0, 0, 0, 9)).stateFlag, 9);
	EXPECT_EQ(decodeGatherableInfo(gatherableInfoBytes(2, 3, 0, 0, 0, 10)).stateFlag, 10);
	EXPECT_THROW(decodeGatherableInfo(gatherableInfoBytes(2, 3, 0, 0, 0, 2)), DecodeError);
	std::vector<uint8_t> changedTrailer = body;
	changedTrailer.back() = 99; // Java writes the constant 100
	EXPECT_THROW(decodeGatherableInfo(changedTrailer), DecodeError);
}

TEST(VisibilityDecodersTest, QuestListCarriesTheNegatedEntryCount) {
	EXPECT_EQ(questListBytes({}).size(), 4u);
	EXPECT_TRUE(decodeQuestList(questListBytes({})).quests.empty()) << "V10: a new character has no quest";

	const std::vector<QuestEntry> quests = {{1006, 2, 0x01020304, 3}, {1007, 1, 0, 0}};
	const std::vector<uint8_t> body = questListBytes(quests);
	EXPECT_EQ(body.size(), 4u + 2 * 10u);
	EXPECT_EQ(decodeQuestList(body).quests, quests);

	std::vector<uint8_t> wrongLeader = body;
	wrongLeader[0] = 2; // Java writes the constant 1
	EXPECT_THROW(decodeQuestList(wrongLeader), DecodeError);
}

TEST(VisibilityDecodersTest, MacroListCarriesTheNegatedEntryCount) {
	EXPECT_EQ(macroListBytes(42, true, {}).size(), 7u) << "SM_MACRO_LIST.STATIC_BODY_SIZE";
	const MacroList empty = decodeMacroList(macroListBytes(42, true, {}));
	EXPECT_EQ(empty.playerObjectId, 42);
	EXPECT_TRUE(empty.clearList);
	EXPECT_TRUE(empty.macros.empty());

	const std::vector<MacroEntry> macros = {{1, "<macro/>"}, {7, "<m a=\"1\"/>"}};
	const std::vector<uint8_t> body = macroListBytes(42, false, macros);
	// SM_MACRO_LIST.DYNAMIC_BODY_PART_SIZE_CALCULATOR: 1 + xml.length() * 2 + 2 per macro
	EXPECT_EQ(body.size(), 7u + (1 + 8 * 2 + 2) + (1 + 10 * 2 + 2));
	EXPECT_EQ(decodeMacroList(body).macros, macros);
}

TEST(VisibilityDecodersTest, EmptyWarehouseInfo) {
	const std::vector<uint8_t> body = emptyWarehouseBytes(2, true, 0);
	EXPECT_EQ(body.size(), 7u);
	const WarehouseInfo info = decodeWarehouseInfo(body);
	EXPECT_EQ(info.warehouseType, 2);
	EXPECT_TRUE(info.firstPacket);
	EXPECT_EQ(info.expandLevel, 0);
	EXPECT_FALSE(info.regularWithItems);
	EXPECT_TRUE(info.items.empty());

	std::vector<uint8_t> unexpectedBranch = body;
	unexpectedBranch[3] = 2; // neither writeH(0) nor the writeC(1)/writeC(0) branch
	EXPECT_THROW(decodeWarehouseInfo(unexpectedBranch), DecodeError);
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeWarehouseInfo(trailing), DecodeError);
}

TEST(VisibilityDecodersTest, WarehouseInfoReadsAnItemWithItsInfoBlob) {
	// the regular warehouse with at least one item writes writeC(1), writeC(0) where the other cases write a zero short
	PacketWriter w;
	w.C(1).C(1).C(0);
	w.C(1).C(0);
	w.H(1); // one item
	w.D(0x50000001).D(182400001);
	w.C(0); // "some item info"
	w.S("Kinah");
	// ItemInfoBlob with a single GENERAL_INFO entry (1 + 2 + 8 + 2 + 1 + 4 + 4 + 4 + 2 + 4 + 2 = 34 bytes)
	PacketWriter blob;
	blob.C(0x00);   // GENERAL_INFO
	blob.H(0x0001); // item mask
	blob.Q(1000);   // count (long)
	blob.S("");     // creator
	blob.C(0);      // byte after the creator
	blob.D(0);      // seconds until expiration
	blob.D(0);      // disappear time
	blob.D(0);      // temporary exchange time
	blob.H(0);      // seal status
	blob.D(0);      // remaining unsealing time
	blob.H(18);     // 4.7.5
	w.H(static_cast<int32_t>(blob.data.size())).B(blob.data);
	w.H(0xFFFF); // not equipped
	const WarehouseInfo info = decodeWarehouseInfo(w.data);
	ASSERT_EQ(info.items.size(), 1u);
	EXPECT_TRUE(info.regularWithItems);
	EXPECT_EQ(info.items[0].templateId, 182400001);
	EXPECT_EQ(info.items[0].l10n, "Kinah");
	ASSERT_TRUE(info.items[0].general);
	EXPECT_EQ(info.items[0].general->count, 1000);
	EXPECT_EQ(info.items[0].equipmentSlot, 0xFFFF);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
