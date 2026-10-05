// The G-02 travel decoders (m5f-plan.md §2.1-§2.5, G-02) against byte vectors written from the Java writeImpl, in the style of
// ProgressionDecodersTest.cpp: a body built field by field in Java order, the body size Java produces, every constant the decoder verifies
// changed once, and a truncated and an over-long body refused. The values are the gate's own (m5f-plan.md §10.3): Kustanon's map (T5),
// Daines' jump to Verteron (T10), the hotspot's three actions (T1, T9), the obelisk's bind (T6), Haramel's instance info and count (T13), its
// use bar (T13), the observer's SM_DELETE(11) (T10) and STR_MSG_LEAVE_INSTANCE(0) (T15). No case includes or consults a C++ serverpackets
// header (m5a-plan.md D9).

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

#include "decoders/TravelDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** every decoder refuses a body one byte short and one byte long */
template <typename Decode>
void expectExactLength(const std::vector<uint8_t>& body, Decode decode, const char* name) {
	ASSERT_FALSE(body.empty());
	std::vector<uint8_t> shorter(body.begin(), body.end() - 1);
	EXPECT_THROW(decode(shorter), DecodeError) << name << " one byte short";
	std::vector<uint8_t> longer = body;
	longer.push_back(0);
	EXPECT_THROW(decode(longer), DecodeError) << name << " one byte long";
}

TEST(TravelDecodersTest, TeleportMapOfKustanon) {
	PacketWriter w;
	w.D(0x40001234); // writeD(targetObjId)
	w.H(103);        // writeH(teleportId)
	ASSERT_EQ(w.data.size(), 6u);
	const TeleportMap map = decodeTeleportMap(w.data);
	EXPECT_EQ(map.targetObjectId, 0x40001234);
	EXPECT_EQ(map.teleportId, 103);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeTeleportMap(b); }, "SM_TELEPORT_MAP");
}

TEST(TravelDecodersTest, TeleportLocOfTheJumpToVerteronAndOfTheBeamIntoAnInstance) {
	PacketWriter w;
	w.C(TELEPORT_ANIMATION_JUMP_IN).D(210030000).D(210030000).F(1640.76f).F(1500.32f).F(119.71f).C(0);
	ASSERT_EQ(w.data.size(), 22u);
	const TeleportLoc loc = decodeTeleportLoc(w.data);
	EXPECT_EQ(loc.animation, 3);
	EXPECT_EQ(loc.mapId, 210030000);
	EXPECT_EQ(loc.mapOrInstanceId, 210030000);
	EXPECT_FLOAT_EQ(loc.x, 1640.76f);
	EXPECT_FLOAT_EQ(loc.y, 1500.32f);
	EXPECT_FLOAT_EQ(loc.z, 119.71f);
	EXPECT_EQ(loc.heading, 0);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeTeleportLoc(b); }, "SM_TELEPORT_LOC");

	PacketWriter beam;
	beam.C(TELEPORT_ANIMATION_FADE_OUT_BEAM).D(300200000).D(2).F(172.0f).F(20.0f).F(144.22548f).C(60);
	const TeleportLoc instance = decodeTeleportLoc(beam.data);
	EXPECT_EQ(instance.animation, 1);
	EXPECT_EQ(instance.mapOrInstanceId, 2) << "an instance map writes the instance id";
	EXPECT_EQ(instance.heading, 60);
}

TEST(TravelDecodersTest, ChannelInfoOfANotSpawnedPosition) {
	PacketWriter w;
	w.D(1).D(1); // the constructor's (1, 1) for a position that is not spawned
	const ChannelInfo info = decodeChannelInfo(w.data);
	EXPECT_EQ(info.currentChannel, 1);
	EXPECT_EQ(info.instanceCount, 1);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeChannelInfo(b); }, "SM_CHANNEL_INFO");
}

TEST(TravelDecodersTest, BindPointTeleportOfEachAction) {
	PacketWriter cast;
	cast.C(1).D(0x1001).D(13); // case 1: writeD(locId)
	ASSERT_EQ(cast.data.size(), 9u);
	BindPointTeleport one = decodeBindPointTeleport(cast.data);
	EXPECT_EQ(one.action, 1);
	EXPECT_EQ(one.playerId, 0x1001);
	EXPECT_EQ(one.locId, 13);
	EXPECT_EQ(one.cooldown, 0);
	expectExactLength(cast.data, [](std::span<const uint8_t> b) { return decodeBindPointTeleport(b); }, "SM_BIND_POINT_TELEPORT(1)");

	PacketWriter cancel;
	cancel.C(2).D(0x1001); // no case 2 in the switch: nothing more
	ASSERT_EQ(cancel.data.size(), 5u);
	EXPECT_EQ(decodeBindPointTeleport(cancel.data).action, 2);
	std::vector<uint8_t> longer = cancel.data;
	longer.push_back(0);
	EXPECT_THROW(decodeBindPointTeleport(longer), DecodeError);

	PacketWriter done;
	done.C(3).D(0x1001).D(13).D(600); // case 3: writeD(locId), writeD(cooldown)
	ASSERT_EQ(done.data.size(), 13u);
	const BindPointTeleport three = decodeBindPointTeleport(done.data);
	EXPECT_EQ(three.locId, 13);
	EXPECT_EQ(three.cooldown, 600);
	expectExactLength(done.data, [](std::span<const uint8_t> b) { return decodeBindPointTeleport(b); }, "SM_BIND_POINT_TELEPORT(3)");
}

TEST(TravelDecodersTest, BindPointInfoOfAnObelisk) {
	PacketWriter w;
	w.C(0).C(1).D(210010000).F(425.0f).F(1741.0f).F(120.5f).D(0);
	ASSERT_EQ(w.data.size(), 22u);
	const BindPointInfo info = decodeBindPointInfo(w.data);
	EXPECT_EQ(info.type, 0);
	EXPECT_EQ(info.mapId, 210010000);
	EXPECT_FLOAT_EQ(info.x, 425.0f);
	EXPECT_FLOAT_EQ(info.y, 1741.0f);
	EXPECT_FLOAT_EQ(info.z, 120.5f);
	EXPECT_EQ(info.kiskObjectId, 0);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeBindPointInfo(b); }, "SM_BIND_POINT_INFO");
	std::vector<uint8_t> changed = w.data;
	changed[1] = 0; // the literal writeC(0x01)
	EXPECT_THROW(decodeBindPointInfo(changed), DecodeError);
	changed = w.data;
	changed[0] = 2; // neither 0 nor 4
	EXPECT_THROW(decodeBindPointInfo(changed), DecodeError);
}

TEST(TravelDecodersTest, InstanceInfoOfHaramelAfterTheFirstEntry) {
	PacketWriter w;
	w.C(2).D(46).C(0).H(1);      // updateType 2, the one instance's cooltime id, unk1, one player
	w.D(0x2002).H(1);            // the player, one instance
	w.D(46).D(0).D(43200).D(16).D(-1).C(1); // id, 0, reuse seconds, max, -enterCount, show
	w.S("Travelerb");
	const InstanceInfo info = decodeInstanceInfo(w.data);
	EXPECT_EQ(info.updateType, 2);
	EXPECT_EQ(info.cooltimeId, 46);
	ASSERT_EQ(info.players.size(), 1u);
	EXPECT_EQ(info.players[0].objectId, 0x2002);
	EXPECT_EQ(info.players[0].name, "Travelerb");
	const std::optional<InstanceCooldownEntry> haramel = info.entry(46);
	ASSERT_TRUE(haramel);
	EXPECT_EQ(haramel->reuseSeconds, 43200);
	EXPECT_EQ(haramel->maxCount, 16);
	EXPECT_EQ(haramel->entryOffset, -1);
	EXPECT_EQ(haramel->show, 1);
	EXPECT_FALSE(info.entry(47));
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeInstanceInfo(b); }, "SM_INSTANCE_INFO");
	std::vector<uint8_t> changed = w.data;
	changed[5] = 1; // the literal unk1
	EXPECT_THROW(decodeInstanceInfo(changed), DecodeError);
	changed = w.data;
	changed[18] = 1; // the literal writeD(0x00) after the cooltime id
	EXPECT_THROW(decodeInstanceInfo(changed), DecodeError);
}

TEST(TravelDecodersTest, InstanceCountInfoOfASoloInstance) {
	PacketWriter w;
	w.D(300200000).D(2).D(1);
	const InstanceCountInfo info = decodeInstanceCountInfo(w.data);
	EXPECT_EQ(info.mapId, 300200000);
	EXPECT_EQ(info.instanceId, 2);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeInstanceCountInfo(b); }, "SM_INSTANCE_COUNT_INFO");
	std::vector<uint8_t> changed = w.data;
	changed[8] = 31; // "1 solo 31 group"
	EXPECT_THROW(decodeInstanceCountInfo(changed), DecodeError);
}

TEST(TravelDecodersTest, UseObjectOfThePortalBar) {
	PacketWriter w;
	w.D(0x2002).D(0x40005678).D(3000).C(USE_OBJECT_START_BAR);
	ASSERT_EQ(w.data.size(), 13u);
	const UseObject use = decodeUseObject(w.data);
	EXPECT_EQ(use.playerObjectId, 0x2002);
	EXPECT_EQ(use.targetObjectId, 0x40005678);
	EXPECT_EQ(use.time, 3000);
	EXPECT_EQ(use.actionType, 1);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeUseObject(b); }, "SM_USE_OBJECT");
}

TEST(TravelDecodersTest, DeleteWithTheJumpAnimation) {
	PacketWriter w;
	w.D(0x2002).C(DELETE_ANIMATION_JUMP_IN);
	const Delete deleted = decodeDelete(w.data);
	EXPECT_EQ(deleted.objectId, 0x2002);
	EXPECT_EQ(deleted.animation, 11);
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeDelete(b); }, "SM_DELETE");
}

TEST(TravelDecodersTest, SystemMessageWithOneParameter) {
	PacketWriter w;
	w.C(0x13).C(0).D(0).D(1400208).C(1).S("0").C(0);
	const SystemMessage message = decodeSystemMessage(w.data);
	EXPECT_EQ(message.chatType, 0x13);
	EXPECT_EQ(message.messageId, 1400208);
	ASSERT_EQ(message.params.size(), 1u);
	EXPECT_EQ(message.params[0], "0");
	EXPECT_TRUE(message.specialParams.empty());
	expectExactLength(w.data, [](std::span<const uint8_t> b) { return decodeSystemMessage(b); }, "SM_SYSTEM_MESSAGE");
	std::vector<uint8_t> changed = w.data;
	changed[1] = 1; // the literal writeC(0x00)
	EXPECT_THROW(decodeSystemMessage(changed), DecodeError);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
