// GameSession (m5a-plan.md F-04): the client packet bodies read back field by field in the order of the Java readImpl methods
// (clientpackets/CM_*.java, AbstractCharacterEditPacket.java, AionClientPacket.readS(int)), the fixed MAC address and HDD serial, and the
// packet names of the recorder.

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include "GameSession.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using network::test::PacketReader;

TEST(GameSessionTest, VersionCheckAndLoginCheck) {
	std::vector<uint8_t> version = GameSession::buildCM_VERSION_CHECK();
	PacketReader v(version);
	EXPECT_EQ(static_cast<uint16_t>(v.H()), GameSession::CLIENT_VERSION); // aionClientVersion
	v.H();                                                                // npcScriptInterfaceVersion
	v.D();                                                                // windowsEncoding
	v.D();                                                                // windowsVersion
	v.D();                                                                // windowsSubVersion
	v.C();                                                                // liteInfo
	EXPECT_EQ(v.remaining(), 0u);

	std::vector<uint8_t> login = GameSession::buildCM_L2AUTH_LOGIN_CHECK(11, 22, 33, 44);
	PacketReader l(login);
	EXPECT_EQ(l.D(), 11); // playOk2
	EXPECT_EQ(l.D(), 22); // playOk1
	EXPECT_EQ(l.D(), 33); // accountId
	EXPECT_EQ(l.D(), 44); // loginOk
	l.D();
	l.D();
	EXPECT_EQ(l.remaining(), 0u);
}

TEST(GameSessionTest, MacAddressCarriesTheFixedMacAndSerial) {
	// LoginServer.java validates the MAC against ([0-9A-F]{2}-){5}[0-9A-F]{2}; CM_MAC_ADDRESS.fixHddSerial keeps a serial of [0-9a-zA-Z _-] longer
	// than 2 characters without a space at the second or penultimate position unchanged
	EXPECT_TRUE(std::regex_match(std::string(GameSession::MAC_ADDRESS), std::regex("([0-9A-F]{2}-){5}[0-9A-F]{2}")));
	EXPECT_TRUE(std::regex_match(std::string(GameSession::HDD_SERIAL), std::regex("[0-9a-zA-Z_-]{3,}")));

	std::vector<uint8_t> mac = GameSession::buildCM_MAC_ADDRESS();
	PacketReader m(mac);
	m.C();                         // unk
	int32_t routeSteps = m.H();    // readUH
	for (int32_t i = 0; i < routeSteps; i++)
		m.D();                     // traceroute ips
	EXPECT_EQ(m.S(), "0A-1B-2C-3D-4E-5F");
	EXPECT_EQ(m.S(), GameSession::HDD_SERIAL);
	m.D();                         // local IP
	EXPECT_EQ(m.remaining(), 0u);
}

TEST(GameSessionTest, SmallBodies) {
	EXPECT_EQ(GameSession::buildCM_TIME_CHECK(7), (std::vector<uint8_t>{7, 0, 0, 0}));
	EXPECT_EQ(GameSession::buildCM_CHARACTER_LIST(0x01020304), (std::vector<uint8_t>{4, 3, 2, 1}));
	EXPECT_EQ(GameSession::buildCM_PING(), (std::vector<uint8_t>{0, 0}));
	EXPECT_EQ(GameSession::buildCM_GAMEGUARD(std::vector<uint8_t>{9, 8}), (std::vector<uint8_t>{2, 0, 0, 0, 9, 8}));
	EXPECT_TRUE(GameSession::buildCM_SECURITY_TOKEN().empty());
	EXPECT_TRUE(GameSession::buildCM_MAY_LOGIN_INTO_GAME().empty());
	EXPECT_TRUE(GameSession::buildCM_LEVEL_READY().empty());
	EXPECT_EQ(GameSession::buildCM_ENTER_WORLD(0x100), (std::vector<uint8_t>{0, 1, 0, 0}));
	EXPECT_EQ(GameSession::buildCM_QUIT(true), (std::vector<uint8_t>{1}));
	EXPECT_EQ(GameSession::buildCM_QUIT(false), (std::vector<uint8_t>{0}));
	PacketReader nick(GameSession::buildCM_CHECK_NICKNAME("Elyostest"));
	EXPECT_EQ(nick.S(), "Elyostest");
	EXPECT_EQ(nick.remaining(), 0u);
}

TEST(GameSessionTest, CreateCharacterLayout) {
	NewCharacter character;
	character.name = "Scenario";
	character.female = true;
	character.asmodian = false;
	character.playerClassId = NewCharacter::MAGE;
	character.appearance.face = 3;
	character.appearance.facialRate = 7;
	character.appearance.height = 1.25f;
	std::vector<uint8_t> body = GameSession::buildCM_CREATE_CHARACTER(5, "account", character, 0);
	PacketReader r(body);
	EXPECT_EQ(r.D(), 5);         // account id
	EXPECT_EQ(r.S(), "account"); // account name
	// readS(25): the string with its terminator, then (25 - 8) * 2 bytes
	EXPECT_EQ(r.S(), "Scenario");
	EXPECT_EQ(r.B(34), std::vector<uint8_t>(34, 0));
	EXPECT_EQ(r.D(), 1); // gender: female
	EXPECT_EQ(r.D(), 0); // race: elyos
	EXPECT_EQ(r.D(), 6); // PlayerClass.MAGE
	for (int i = 0; i < 5; i++)
		r.D();                   // voice, skin, hair, eye and lip colors
	EXPECT_EQ(r.C(), 3);         // face
	for (int i = 0; i < 5; i++)
		r.C();                   // hair, deco, tattoo, face contour, expression
	EXPECT_EQ(r.C(), 4);         // "always 4"
	// jaw line .. foot size: 2 + 6 + 3 + 4 + 10 + 2 + 1 + 4 + 1 + 2 + 1 bytes
	r.B(36);
	EXPECT_EQ(r.C(), 7);         // facial rate
	EXPECT_EQ(r.C(), 0);         // "always 0"
	r.B(4);                      // arm length, leg length, shoulders, face shape
	r.B(3);                      // three unknown bytes
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(r.D())), 1.25f); // height
	EXPECT_EQ(r.C(), 0);         // type
	EXPECT_EQ(r.remaining(), 0u);
}

TEST(GameSessionTest, MoveLayouts) {
	constexpr int8_t positionManualAbsolute = static_cast<int8_t>(0x80 | 0x40 | 0x20);
	PacketReader move(GameSession::buildCM_MOVE(1.0f, 2.0f, 3.0f, 60, positionManualAbsolute, 4.0f, 5.0f, 6.0f));
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(move.D())), 1.0f);
	move.D();
	move.D();
	EXPECT_EQ(move.C(), 60);
	EXPECT_EQ(static_cast<int8_t>(move.C()), positionManualAbsolute);
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(move.D())), 4.0f); // x2 (ABSOLUTE)
	move.D();
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(move.D())), 6.0f);
	EXPECT_EQ(move.remaining(), 0u);
	EXPECT_EQ(GameSession::buildCM_MOVE(1.0f, 2.0f, 3.0f, 0, 0).size(), 14u); // stop move: position, heading, type
}

TEST(GameSessionTest, ServerPacketNames) {
	EXPECT_EQ(GameSession::nameOf(0), "SM_VERSION_CHECK");
	EXPECT_EQ(GameSession::nameOf(14), "SM_NPC_INFO");
	EXPECT_EQ(GameSession::nameOf(15), "SM_PLAYER_SPAWN");
	EXPECT_EQ(GameSession::nameOf(72), "SM_KEY");
	EXPECT_EQ(GameSession::nameOf(9), "SM_UNKNOWN_9");
}

} // namespace
} // namespace aion::gameserver::scenario
