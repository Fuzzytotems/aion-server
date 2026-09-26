// readImpl byte vectors of the login slice client packets (P5-00, m5a-plan.md S-01/S-03/S-05/S-07/S-08/S-09/S-10): every body below is laid
// out field by field from the Java readImpl, and read() must consume it exactly (the base class warns once per opcode about unread bytes and
// logs "Missing X" on underflow). What the fields are used for is checked by the flow tests (tests/network/LoginSliceFlowTest.cpp).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/controllers/movement/GlideFlag.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHARACTER_LIST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHARACTER_PASSKEY.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_NICKNAME.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CREATE_CHARACTER.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_CHARACTER.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DISCONNECT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_ENTER_WORLD.h"
#include "aion/gameserver/network/aion/clientpackets/CM_GAMEGUARD.h"
#include "aion/gameserver/network/aion/clientpackets/CM_L2AUTH_LOGIN_CHECK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_LEVEL_READY.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MAC_ADDRESS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MAY_LOGIN_INTO_GAME.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MOVE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PING.h"
#include "aion/gameserver/network/aion/clientpackets/CM_QUIT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_RECONNECT_AUTH.h"
#include "aion/gameserver/network/aion/clientpackets/CM_RESTORE_CHARACTER.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SECURITY_TOKEN.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TIME_CHECK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_UI_SETTINGS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_VERSION_CHECK.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "../support/NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {
namespace {

using controllers::movement::GlideFlag;
using controllers::movement::MovementMask;
using test::LogCapture;
using test::PacketWriter;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** Reads `data` with a fresh packet of type P (no connection) and returns the unread byte count, -1 if read() failed */
template <class P>
int32_t unreadBytesAfterRead(const std::vector<uint8_t>& data, int32_t opcode = 1) {
	std::vector<uint8_t> copy = data;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST)); // readImpl may create runtime objects (the appearance), like on the IO strand
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::AUTHED});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return -1;
	return packet->getRemainingBytes();
}

/** Asserts that P consumes `data` exactly and logs no missing field */
template <class P>
void expectExactRead(const std::vector<uint8_t>& data, int32_t opcode) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	EXPECT_EQ(unreadBytesAfterRead<P>(data, opcode), 0) << typeid(P).name();
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ClientPacketReadTest, LoginPackets) {
	// CM_VERSION_CHECK: UH version, UH npc script version, D encoding, D windows version, D sub version, C lite info
	expectExactRead<CM_VERSION_CHECK>(PacketWriter().H(0x0C3A).H(1).D(949).D(10).D(0).C(2).data, 100);
	// CM_TIME_CHECK: D nano time
	expectExactRead<CM_TIME_CHECK>(PacketWriter().D(123456).data, 101);
	// CM_L2AUTH_LOGIN_CHECK: D playOk2, D playOk1, D accountId, D loginOk, D unk1, D unk2
	expectExactRead<CM_L2AUTH_LOGIN_CHECK>(PacketWriter().D(1).D(2).D(3).D(4).D(5).D(6).data, 102);
	// CM_MAC_ADDRESS: C unk, UH route steps, D per step, S mac, S hdd serial, D local ip
	expectExactRead<CM_MAC_ADDRESS>(PacketWriter().C(0).H(2).D(0x0100007F).D(0x0101A8C0).S("0A-1B-2C-3D-4E-5F").S("WD-WCAV12345678").D(7).data, 103);
	expectExactRead<CM_MAC_ADDRESS>(PacketWriter().C(0).H(0).S("").S("").D(0).data, 104);
	// CM_CHARACTER_LIST: D playOk2
	expectExactRead<CM_CHARACTER_LIST>(PacketWriter().D(77).data, 105);
	// CM_MAY_LOGIN_INTO_GAME, CM_SECURITY_TOKEN, CM_RECONNECT_AUTH, CM_LEVEL_READY: empty bodies
	expectExactRead<CM_MAY_LOGIN_INTO_GAME>({}, 106);
	expectExactRead<CM_SECURITY_TOKEN>({}, 107);
	expectExactRead<CM_RECONNECT_AUTH>({}, 108);
	expectExactRead<CM_LEVEL_READY>({}, 109);
	// CM_PING: H unk
	expectExactRead<CM_PING>(PacketWriter().H(0).data, 110);
	// CM_GAMEGUARD: D size, B size
	expectExactRead<CM_GAMEGUARD>(PacketWriter().D(3).C(1).C(2).C(3).data, 111);
	// CM_DISCONNECT: C flag
	expectExactRead<CM_DISCONNECT>(PacketWriter().C(0).data, 112);
	// CM_UI_SETTINGS: C type, H unk, UH size, B rest
	expectExactRead<CM_UI_SETTINGS>(PacketWriter().C(0).H(0).H(4).D(0x04030201).data, 113);
}

TEST(ClientPacketReadTest, PasskeyReadsFixed48ByteFields) {
	// type 0/3: H type, B(48) passkey; type 2: a second B(48)
	expectExactRead<CM_CHARACTER_PASSKEY>(PacketWriter().H(3).zeros(48).data, 120);
	expectExactRead<CM_CHARACTER_PASSKEY>(PacketWriter().H(2).zeros(96).data, 121);
	// with type 3 a second field is not read
	EXPECT_EQ(unreadBytesAfterRead<CM_CHARACTER_PASSKEY>(PacketWriter().H(3).zeros(96).data), 48);
}

TEST(ClientPacketReadTest, CharacterSelectionPackets) {
	// CM_CHECK_NICKNAME: S nick
	expectExactRead<CM_CHECK_NICKNAME>(PacketWriter().S("Tester").data, 130);
	// CM_DELETE_CHARACTER / CM_RESTORE_CHARACTER: D playOk2, D object id
	expectExactRead<CM_DELETE_CHARACTER>(PacketWriter().D(1).D(2).data, 131);
	expectExactRead<CM_RESTORE_CHARACTER>(PacketWriter().D(1).D(2).data, 132);
	// CM_ENTER_WORLD: D object id
	expectExactRead<CM_ENTER_WORLD>(PacketWriter().D(99).data, 133);
	// CM_QUIT: C stay connected
	expectExactRead<CM_QUIT>(PacketWriter().C(1).data, 134);
}

/** The body of CM_CREATE_CHARACTER after the account fields, from AbstractCharacterEditPacket.readBasicInfo/readAppearance */
std::vector<uint8_t> createCharacterBody(std::string_view name, int32_t gender, int32_t race, int32_t playerClass, int32_t type) {
	PacketWriter w;
	w.D(5).S("account"); // readD account id, readS account name
	// readS(25): the name with its terminating 0 char, then (25 - length) * 2 padding bytes
	w.S(name).zeros(static_cast<size_t>(25 - static_cast<int32_t>(name.size())) * 2);
	w.D(gender).D(race).D(playerClass);
	w.D(1).D(2).D(3).D(4).D(5);   // voice, skin, hair, eye, lip RGB
	for (int i = 0; i < 6; i++)   // face, hair, deco, tattoo, face contour, expression
		w.C(i);
	w.C(4);                       // always 4
	for (int i = 0; i < 37; i++)  // jaw line ... facial rate
		w.C(10 + i);
	w.C(0);                       // always 0
	for (int i = 0; i < 4; i++)   // arm length, leg length, shoulders, face shape
		w.C(60 + i);
	w.C(0).C(0).C(0);             // 3 unknown bytes
	w.F(1.25f);                   // height
	w.C(type);                    // type
	return w.data;
}

TEST(ClientPacketReadTest, CreateCharacterLayout) {
	expectExactRead<CM_CREATE_CHARACTER>(createCharacterBody("Tester", 0, 0, 0, 0), 140);
	// type 1 (entering the creation screen): the client sends random data; an invalid class is ignored (Java PlayerClass null)
	expectExactRead<CM_CREATE_CHARACTER>(createCharacterBody("x", 1, 1, 77, 1), 141);
	// one byte short: the type is missing
	std::vector<uint8_t> shortBody = createCharacterBody("Tester", 0, 0, 0, 0);
	shortBody.pop_back();
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	EXPECT_EQ(unreadBytesAfterRead<CM_CREATE_CHARACTER>(shortBody, 142), 0);
	EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
}

TEST(ClientPacketReadTest, MoveReadsTheOptionalBlocksOfTheMovementMask) {
	const int32_t positionManual = static_cast<uint8_t>(MovementMask::POSITION | MovementMask::MANUAL);
	// stop: F x, F y, F z, C heading, C type
	expectExactRead<CM_MOVE>(PacketWriter().F(1).F(2).F(3).C(10).C(MovementMask::IMMEDIATE).data, 150);
	// POSITION|MANUAL without ABSOLUTE: 3 vector floats
	expectExactRead<CM_MOVE>(PacketWriter().F(1).F(2).F(3).C(10).C(positionManual).F(0.5f).F(0.5f).F(0).data, 151);
	// POSITION|MANUAL|ABSOLUTE: 3 target floats
	expectExactRead<CM_MOVE>(
		PacketWriter().F(1).F(2).F(3).C(10).C(positionManual | static_cast<uint8_t>(MovementMask::ABSOLUTE)).F(4).F(5).F(6).data, 152);
	// GLIDE with the geyser flag: C glide flag, UC location id
	expectExactRead<CM_MOVE>(
		PacketWriter().F(1).F(2).F(3).C(10).C(static_cast<uint8_t>(MovementMask::GLIDE)).C(static_cast<uint8_t>(GlideFlag::GEYSER)).C(7).data, 153);
	// GLIDE without the geyser flag: only the glide flag
	expectExactRead<CM_MOVE>(PacketWriter().F(1).F(2).F(3).C(10).C(static_cast<uint8_t>(MovementMask::GLIDE)).C(0).data, 154);
	// VEHICLE: D, D, F, F, F
	expectExactRead<CM_MOVE>(PacketWriter().F(1).F(2).F(3).C(10).C(static_cast<uint8_t>(MovementMask::VEHICLE)).D(1).D(2).F(3).F(4).F(5).data, 155);
	// POSITION alone reads no vector
	expectExactRead<CM_MOVE>(PacketWriter().F(1).F(2).F(3).C(10).C(static_cast<uint8_t>(MovementMask::POSITION)).data, 156);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets
