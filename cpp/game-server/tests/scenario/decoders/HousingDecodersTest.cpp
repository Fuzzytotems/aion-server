// The M5h studio decoders (m5h-plan.md §2.7, G-03) against byte vectors written from the Java writeImpl, in the style of
// TravelDecodersTest.cpp: a body built field by field in Java order, the body size Java produces, the constants the decoder verifies, and a
// truncated and an over-long body refused. The values are the gate's (m5h-plan.md §10.3 Y13-Y19): A's studio 2001 (building 355000), the cake
// 3190034 with its usage block, the bed 3120000 (a chair, type 5), the wallpaper 3554000 in the first inner-wall room. No case includes or
// consults a C++ serverpackets header (m5a-plan.md D9).

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "decoders/HousingDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

template <typename Decode>
void expectExactLength(const std::vector<uint8_t>& body, Decode decode, const char* name) {
	ASSERT_FALSE(body.empty());
	std::vector<uint8_t> shorter(body.begin(), body.end() - 1);
	EXPECT_THROW(decode(shorter), DecodeError) << name << " one byte short";
	std::vector<uint8_t> longer = body;
	longer.push_back(0);
	EXPECT_THROW(decode(longer), DecodeError) << name << " one byte long";
}

/** AionServerPacket.writeS(text, fixedLength): fixedLength chars, NUL padded, then a NUL char; an empty text is (fixedLength + 1) * 2 zeros */
void fixedString(PacketWriter& w, const std::string& text, size_t fixedLength) {
	for (size_t i = 0; i < fixedLength; i++)
		w.H(i < text.size() ? static_cast<uint8_t>(text[i]) : 0);
	w.H(0);
}

/** writeCommonInfo of A's studio after the wallpaper (C16) and the settings (C18): closed door, the notice, no legion emblem */
void commonInfo(PacketWriter& w) {
	w.D(0).D(2001).D(1234).D(0).C(1); // writeD(0), address, owner, building type id (PERSONAL_INS 0), writeC(1)
	w.D(355000).C(5).C(3);            // building, owner states, door state CLOSED
	fixedString(w, "Legionfounder", 25);
	w.D(0x40000010).C(0); // legion id, show owner name
	fixedString(w, "keep out", 64);
	for (int i = 0; i < 19; i++)
		w.D(i == 6 ? 3554000 : 0); // INWALL_ANY room 0 is the seventh slot
	w.D(0).D(0).C(0);
	w.C(0).C(0).C(0).C(0).C(0).C(0); // writeB(new byte[6])
}

TEST(HousingDecodersTest, HouseAcquireOfAsStudio) {
	PacketWriter w;
	w.D(1234).D(2001).D(1);
	const HouseAcquire acquire = decodeHouseAcquire(w.data);
	EXPECT_EQ(acquire.playerId, 1234);
	EXPECT_EQ(acquire.address, 2001);
	EXPECT_EQ(acquire.acquire, 1);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeHouseAcquire(b); }, "SM_HOUSE_ACQUIRE");
}

TEST(HousingDecodersTest, HouseOwnerInfoBeforeAndAfterTheStudio) {
	PacketWriter before;
	before.D(0).D(0).C(2).C(0).D(0).D(0).D(0).D(0);
	const HouseOwnerInfo none = decodeHouseOwnerInfo(before.data);
	EXPECT_EQ(none.address, 0);
	EXPECT_EQ(none.ownerState, 2);
	PacketWriter after;
	after.D(2001).D(355000).C(5).C(0).D(0).D(0).D(0).D(0);
	const HouseOwnerInfo owner = decodeHouseOwnerInfo(after.data);
	EXPECT_EQ(owner.address, 2001);
	EXPECT_EQ(owner.buildingId, 355000);
	EXPECT_EQ(owner.ownerState, 5);
	expectExactLength(after.data, [](std::span<const uint8_t> b) { return decodeHouseOwnerInfo(b); }, "SM_HOUSE_OWNER_INFO");
}

TEST(HousingDecodersTest, HouseRenderAndUpdateCarryTheCommonInfo) {
	PacketWriter render;
	commonInfo(render);
	ASSERT_EQ(render.data.size(), 4u * 5 + 1 + 2 + 52 + 5 + 130 + 19 * 4 + 9 + 6);
	const HouseInfo info = decodeHouseRender(render.data);
	EXPECT_EQ(info.address, 2001);
	EXPECT_EQ(info.ownerId, 1234);
	EXPECT_EQ(info.buildingId, 355000);
	EXPECT_EQ(info.doorState, 3);
	EXPECT_EQ(info.ownerName, "Legionfounder");
	EXPECT_EQ(info.legionId, 0x40000010);
	EXPECT_EQ(info.signNotice, "keep out");
	ASSERT_EQ(info.decorIds.size(), HOUSE_DECOR_SLOTS);
	EXPECT_EQ(info.decorIds[6], 3554000);
	expectExactLength(render.data, [](std::span<const uint8_t> b) { return decodeHouseRender(b); }, "SM_HOUSE_RENDER");

	PacketWriter update;
	update.H(1).H(0).H(1);
	commonInfo(update);
	EXPECT_EQ(decodeHouseUpdate(update.data).decorIds[6], 3554000);
	expectExactLength(update.data, [](std::span<const uint8_t> b) { return decodeHouseUpdate(b); }, "SM_HOUSE_UPDATE");
	PacketWriter wrongPrefix;
	wrongPrefix.H(0).H(0).H(1);
	commonInfo(wrongPrefix);
	EXPECT_THROW(decodeHouseUpdate(wrongPrefix.data), DecodeError) << "the first writeH(1)";
}

TEST(HousingDecodersTest, HouseScriptsWithOneScriptAndAnEmptySlot) {
	const std::vector<uint8_t> content{0x78, 0x01, 0x01, 0x02, 0x00, 0xFD, 0xFF, 0x41, 0x00, 0x00, 0x42, 0x00, 0x43};
	PacketWriter w;
	w.D(2001).H(2);
	w.C(0).H(8 + static_cast<int32_t>(content.size()) + 8).D(static_cast<int32_t>(content.size()) + 8).D(2).B(content);
	for (int i = 0; i < 8; i++)
		w.C(0xCD); // SCRIPT_PADDING: eight times (byte) -51
	w.C(1).H(0);
	const HouseScripts scripts = decodeHouseScripts(w.data, 8);
	EXPECT_EQ(scripts.address, 2001);
	ASSERT_EQ(scripts.scripts.size(), 2u);
	EXPECT_TRUE(scripts.scripts[0].hasData);
	EXPECT_EQ(scripts.scripts[0].compressed, content);
	EXPECT_EQ(scripts.scripts[0].padding, std::vector<uint8_t>(8, 0xCD));
	EXPECT_EQ(scripts.scripts[0].uncompressedSize, 2);
	EXPECT_FALSE(scripts.scripts[1].hasData);
	EXPECT_EQ(scripts.scripts[1].id, 1);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeHouseScripts(b, 8); }, "SM_HOUSE_SCRIPTS");
}

TEST(HousingDecodersTest, HouseObjectOfTheCakeAndOfTheBed) {
	PacketWriter cake;
	cake.D(2001).D(1234).D(0x00500001).D(0x00500001).D(3190034).F(366.0f).F(295.5f).F(222.35f).H(90);
	cake.D(0).D(2591990).D(0).D(0).C(1).D(0).C(0); // cooldown, expiration, writeDyeInfo(null), writeD(0), type 1, usage (0 uses, check 0)
	const HouseObjectInfo info = decodeHouseObject(cake.data);
	EXPECT_EQ(info.objectId, 0x00500001);
	EXPECT_EQ(info.templateId, 3190034);
	EXPECT_FLOAT_EQ(info.y, 295.5f);
	EXPECT_EQ(info.rotation, 90);
	EXPECT_EQ(info.secondsUntilExpiration, 2591990);
	ASSERT_TRUE(info.usage);
	EXPECT_EQ(info.usage->useCount, 0);
	expectExactLength(cake.data, [](std::span<const uint8_t> b) { return decodeHouseObject(b); }, "SM_HOUSE_OBJECT");

	PacketWriter bed;
	bed.D(2001).D(1234).D(0x00500002).D(0x00500002).D(3120000).F(364.0f).F(293.0f).F(222.35f).H(0).D(0).D(0).D(0).D(0).C(5);
	const HouseObjectInfo chair = decodeHouseObject(bed.data);
	EXPECT_EQ(chair.typeId, 5);
	EXPECT_FALSE(chair.usage);
	PacketWriter twoIds;
	twoIds.D(2001).D(1234).D(1).D(2).D(3120000).F(0).F(0).F(0).H(0).D(0).D(0).D(0).D(0).C(5);
	EXPECT_THROW(decodeHouseObject(twoIds.data), DecodeError) << "the object id is written twice";
}

TEST(HousingDecodersTest, HouseEditOfEveryActionTheGateReads) {
	PacketWriter add;
	add.C(3).C(1).D(0x00500001).D(3190034).D(2592000).D(0).D(0).C(1).D(1234).D(0).C(0);
	const HouseEdit added = decodeHouseEdit(add.data);
	EXPECT_EQ(added.action, 3);
	EXPECT_EQ(added.storeId, 1);
	EXPECT_EQ(added.templateId, 3190034);
	EXPECT_EQ(added.secondsUntilExpiration, 2592000);
	EXPECT_EQ(added.userId, 1234);
	EXPECT_TRUE(added.usage);
	expectExactLength(add.data, [](std::span<const uint8_t> b) { return decodeHouseEdit(b); }, "SM_HOUSE_EDIT(3)");

	PacketWriter decor;
	decor.C(3).C(2).D(0x00500003).D(3554000).D(0).D(0).D(0).C(0);
	const HouseEdit decoration = decodeHouseEdit(decor.data);
	EXPECT_EQ(decoration.storeId, 2);
	EXPECT_FALSE(decoration.usage);

	PacketWriter spawn;
	spawn.C(5).D(2001).D(1234).D(0x00500002).D(3120000).F(364.0f).F(293.0f).F(222.35f).H(45).D(0).D(0).D(0).D(0).C(5);
	const HouseEdit spawned = decodeHouseEdit(spawn.data);
	EXPECT_EQ(spawned.address, 2001);
	EXPECT_EQ(spawned.playerId, 1234);
	EXPECT_FLOAT_EQ(spawned.x, 364.0f);
	EXPECT_EQ(spawned.rotation, 45);
	expectExactLength(spawn.data, [](std::span<const uint8_t> b) { return decodeHouseEdit(b); }, "SM_HOUSE_EDIT(5)");

	PacketWriter removed;
	removed.C(4).C(2).D(0x00500003);
	EXPECT_EQ(decodeHouseEdit(removed.data).objectId, 0x00500003);
	PacketWriter despawn;
	despawn.C(7).D(0x00500002);
	EXPECT_EQ(decodeHouseEdit(despawn.data).objectId, 0x00500002);
	PacketWriter mode;
	mode.C(1);
	EXPECT_EQ(decodeHouseEdit(mode.data).action, 1);
	EXPECT_THROW(decodeHouseEdit(std::vector<uint8_t>{1, 0}), DecodeError);
}

TEST(HousingDecodersTest, DeleteHouseObjectAndObjectUseUpdate) {
	PacketWriter del;
	del.D(0x00500002);
	EXPECT_EQ(decodeDeleteHouseObject(del.data), 0x00500002);
	expectExactLength(del.data, [](std::span<const uint8_t> b) { return decodeDeleteHouseObject(b); }, "SM_DELETE_HOUSE_OBJECT");

	PacketWriter use;
	use.C(1).D(1234).D(1234).D(0x00500001).D(1).C(0);
	const ObjectUseUpdate update = decodeObjectUseUpdate(use.data);
	EXPECT_EQ(update.userId, 1234);
	EXPECT_EQ(update.objectId, 0x00500001);
	EXPECT_EQ(update.useCount, 1);
	expectExactLength(use.data, [](std::span<const uint8_t> b) { return decodeObjectUseUpdate(b); }, "SM_OBJECT_USE_UPDATE");
	PacketWriter postbox;
	postbox.C(3).D(1234).C(1).D(0x00500001);
	EXPECT_THROW(decodeObjectUseUpdate(postbox.data), DecodeError) << "only the UseableItemObject arm";
}

TEST(HousingDecodersTest, LegionMemberListEntryCarriesTheHouse) {
	PacketWriter w;
	w.C(1).H(-1);
	w.D(1234).S("Legionfounder").C(1).D(9).C(0).D(720010000).C(1).S("").S("").D(0).D(2001).D(3).D(1);
	const LegionMemberList list = decodeLegionMemberList(w.data);
	EXPECT_EQ(list.first, 1);
	EXPECT_EQ(list.count, -1);
	ASSERT_EQ(list.members.size(), 1u);
	EXPECT_EQ(list.members[0].name, "Legionfounder");
	EXPECT_EQ(list.members[0].worldId, 720010000);
	EXPECT_EQ(list.members[0].houseAddress, 2001);
	EXPECT_EQ(list.members[0].houseDoorState, 3);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeLegionMemberList(b); }, "SM_LEGION_MEMBERLIST");

	PacketWriter empty;
	empty.C(1).H(0);
	EXPECT_TRUE(decodeLegionMemberList(empty.data).members.empty());
}

TEST(HousingDecodersTest, LegionUpdateMemberCarriesTheMap) {
	PacketWriter w;
	w.D(1234).C(0).C(1).C(9).D(720010000).C(1).D(0).D(1).D(0).S("");
	const LegionUpdateMember update = decodeLegionUpdateMember(w.data);
	EXPECT_EQ(update.objectId, 1234);
	EXPECT_EQ(update.level, 9);
	EXPECT_EQ(update.worldId, 720010000);
	EXPECT_EQ(update.online, 1);
	EXPECT_EQ(update.messageId, 0);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeLegionUpdateMember(b); }, "SM_LEGION_UPDATE_MEMBER");
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
