// GameSession (m5a-plan.md F-04): the client packet bodies read back field by field in the order of the Java readImpl methods
// (clientpackets/CM_*.java, AbstractCharacterEditPacket.java, AionClientPacket.readS(int)), the fixed MAC address and HDD serial, and the
// packet names of the recorder.

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <regex>
#include <stdexcept>
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

TEST(GameSessionTest, FightBodies) {
	// the opcodes of AionClientPacketFactory: packets[5] CM_REVIVE, packets[31] CM_TARGET_SELECT, packets[32] CM_ATTACK
	EXPECT_EQ(GameSession::CM_REVIVE, 5);
	EXPECT_EQ(GameSession::CM_TARGET_SELECT, 31);
	EXPECT_EQ(GameSession::CM_ATTACK, 32);

	// CM_TARGET_SELECT.readImpl: readD targetObjectId, readC selectTargetOfTarget
	PacketReader target(GameSession::buildCM_TARGET_SELECT(0x01020304, true));
	EXPECT_EQ(target.D(), 0x01020304);
	EXPECT_EQ(target.C(), 1);
	EXPECT_EQ(target.remaining(), 0u);
	EXPECT_EQ(GameSession::buildCM_TARGET_SELECT(0), (std::vector<uint8_t>{0, 0, 0, 0, 0})); // the unselect the gate sends

	// CM_ATTACK.readImpl: readD targetObjectId, readUC attackno, readUH time, readUC type
	PacketReader attack(GameSession::buildCM_ATTACK(0x0A0B0C0D, 7, 0x1234, 5));
	EXPECT_EQ(attack.D(), 0x0A0B0C0D);
	EXPECT_EQ(attack.C(), 7);
	EXPECT_EQ(static_cast<uint16_t>(attack.H()), 0x1234);
	EXPECT_EQ(attack.C(), 5);
	EXPECT_EQ(attack.remaining(), 0u);
	EXPECT_EQ(GameSession::buildCM_ATTACK(1), (std::vector<uint8_t>{1, 0, 0, 0, 0, 0, 0, 0})) << "the defaults are attackno 0, time 0, type 0";

	// CM_REVIVE.readImpl: readUC reviveId, one of the ReviveType ids
	EXPECT_EQ(GameSession::buildCM_REVIVE(), (std::vector<uint8_t>{0}));
	EXPECT_EQ(GameSession::BIND_REVIVE, 0);
	EXPECT_EQ(GameSession::buildCM_REVIVE(GameSession::INSTANCE_REVIVE), (std::vector<uint8_t>{6}));
	EXPECT_EQ(GameSession::buildCM_REVIVE(GameSession::OBELISK_REVIVE), (std::vector<uint8_t>{8}));
}

TEST(GameSessionTest, CastBodies) {
	// the opcodes of AionClientPacketFactory: packets[33] CM_CASTSPELL, packets[35] CM_REMOVE_ALTERED_STATE
	EXPECT_EQ(GameSession::CM_CASTSPELL, 33);
	EXPECT_EQ(GameSession::CM_REMOVE_ALTERED_STATE, 35);

	// CM_CASTSPELL.readImpl, the object arm: readUH spellid, readUC level, readUC targetType, readD targetObjectId, readUH hitTime, readD unk
	const std::vector<uint8_t> flameBolt = GameSession::buildCM_CASTSPELL(1282, 1, 0, 0x0A0B0C0D, 1200);
	EXPECT_EQ(flameBolt, (std::vector<uint8_t>{0x02, 0x05, 0x01, 0x00, 0x0D, 0x0C, 0x0B, 0x0A, 0xB0, 0x04, 0x00, 0x00, 0x00, 0x00}))
		<< "1282 as a short, level 1, type 0, the object id, hit time 1200 as a short, the unk int";
	for (const uint8_t type : {uint8_t{3}, uint8_t{4}}) {
		PacketReader r(GameSession::buildCM_CASTSPELL(2864, 2, type, 77, 9));
		EXPECT_EQ(static_cast<uint16_t>(r.H()), 2864);
		EXPECT_EQ(r.C(), 2);
		EXPECT_EQ(r.C(), type);
		EXPECT_EQ(r.D(), 77) << "types 3 and 4 read an object id too (CM_CASTSPELL.java:44-48)";
		EXPECT_EQ(static_cast<uint16_t>(r.H()), 9);
		EXPECT_EQ(r.D(), 0);
		EXPECT_EQ(r.remaining(), 0u);
	}
	EXPECT_THROW(GameSession::buildCM_CASTSPELL(1282, 1, 1, 5), std::invalid_argument) << "type 1 reads a point, not an object id";
	EXPECT_THROW(GameSession::buildCM_CASTSPELL(1282, 1, 2, 5), std::invalid_argument);

	GameSession::CastRequest point;
	point.spellId = 1328;
	point.level = 3;
	point.targetType = 1;
	point.x = 1.0f;
	point.y = 2.0f;
	point.z = 3.0f;
	point.hitTime = 0x0102;
	point.unk = 0x11223344;
	PacketReader p(GameSession::buildCM_CASTSPELL(point));
	EXPECT_EQ(static_cast<uint16_t>(p.H()), 1328);
	EXPECT_EQ(p.C(), 3);
	EXPECT_EQ(p.C(), 1);
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(p.D())), 1.0f); // x (CM_CASTSPELL.java:50)
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(p.D())), 2.0f);
	EXPECT_EQ(std::bit_cast<float>(static_cast<uint32_t>(p.D())), 3.0f);
	EXPECT_EQ(static_cast<uint16_t>(p.H()), 0x0102);
	EXPECT_EQ(p.D(), 0x11223344);
	EXPECT_EQ(p.remaining(), 0u);

	point.targetType = 2;
	EXPECT_EQ(GameSession::buildCM_CASTSPELL(point).size(), 4u + 12u + 32u + 6u) << "arm 2 reads eight more floats (CM_CASTSPELL.java:58-65)";
	point.targetType = 9;
	EXPECT_EQ(GameSession::buildCM_CASTSPELL(point).size(), 4u + 6u) << "a type without an arm reads nothing between the type and the hit time";

	// CM_REMOVE_ALTERED_STATE.readImpl: readUH skillId, readC, readC
	EXPECT_EQ(GameSession::buildCM_REMOVE_ALTERED_STATE(3195), (std::vector<uint8_t>{0x7B, 0x0C, 0x00, 0x00}));
	EXPECT_EQ(GameSession::buildCM_REMOVE_ALTERED_STATE(3573, 0, 1), (std::vector<uint8_t>{0xF5, 0x0D, 0x00, 0x01}));
}

// m5b3-plan.md G-02: the nine client packets of loot and items, read back in the order of their Java readImpl
TEST(GameSessionTest, LootAndItemBodies) {
	// the opcodes, AionClientPacketFactory.java:65, 66, 102, 144, 182-185, 206
	EXPECT_EQ(GameSession::CM_USE_ITEM, 37);
	EXPECT_EQ(GameSession::CM_EQUIP_ITEM, 38);
	EXPECT_EQ(GameSession::CM_MANASTONE, 74);
	EXPECT_EQ(GameSession::CM_DELETE_ITEM, 116);
	EXPECT_EQ(GameSession::CM_START_LOOT, 154);
	EXPECT_EQ(GameSession::CM_LOOT_ITEM, 155);
	EXPECT_EQ(GameSession::CM_MOVE_ITEM, 156);
	EXPECT_EQ(GameSession::CM_SPLIT_ITEM, 157);
	EXPECT_EQ(GameSession::CM_REPLACE_ITEM, 178);

	// CM_START_LOOT: readD targetObjectId, readC action; CM_LOOT_ITEM: readD targetObjectId, readUC index
	EXPECT_EQ(GameSession::buildCM_START_LOOT(0x0A0B0C0D), (std::vector<uint8_t>{0x0D, 0x0C, 0x0B, 0x0A, 0x00})) << "action 0 opens";
	EXPECT_EQ(GameSession::buildCM_START_LOOT(0x0A0B0C0D, GameSession::LOOT_CLOSE), (std::vector<uint8_t>{0x0D, 0x0C, 0x0B, 0x0A, 0x01}));
	EXPECT_EQ(GameSession::buildCM_LOOT_ITEM(0x0A0B0C0D, 200), (std::vector<uint8_t>{0x0D, 0x0C, 0x0B, 0x0A, 0xC8}));

	// CM_USE_ITEM: readD uniqueItemId, readC type, and an int only for types 2, 5 and 6
	EXPECT_EQ(GameSession::buildCM_USE_ITEM(0x01020304), (std::vector<uint8_t>{0x04, 0x03, 0x02, 0x01, 0x00})) << "a potion: type 0, no arm";
	for (const int8_t type : std::initializer_list<int8_t>{2, 5, 6}) {
		PacketReader use(GameSession::buildCM_USE_ITEM(7, type, 0x11223344));
		EXPECT_EQ(use.D(), 7);
		EXPECT_EQ(use.C(), type);
		EXPECT_EQ(use.D(), 0x11223344) << "type " << static_cast<int32_t>(type);
		EXPECT_EQ(use.remaining(), 0u);
	}
	EXPECT_EQ(GameSession::buildCM_USE_ITEM(7, 3, 0x11223344).size(), 5u) << "type 3 has no arm, whatever `extra` says";

	// CM_MOVE_ITEM: readD itemObjId, readC source, readC destination, readH slot
	EXPECT_EQ(GameSession::buildCM_MOVE_ITEM(0x01020304, 0, 1, -1), (std::vector<uint8_t>{0x04, 0x03, 0x02, 0x01, 0x00, 0x01, 0xFF, 0xFF}));

	// CM_SPLIT_ITEM: readD source, readQ amount, readC source storage, readD destination, readC destination storage, readH slot
	PacketReader split(GameSession::buildCM_SPLIT_ITEM(0x0100, 0x123456789A, 0, 0x0200, 1, -1));
	EXPECT_EQ(split.D(), 0x0100);
	EXPECT_EQ(split.Q(), 0x123456789A) << "the amount is a long";
	EXPECT_EQ(split.C(), 0);
	EXPECT_EQ(split.D(), 0x0200);
	EXPECT_EQ(split.C(), 1);
	EXPECT_EQ(split.H(), -1);
	EXPECT_EQ(split.remaining(), 0u);

	// CM_REPLACE_ITEM: readC, readD, readC, readD
	EXPECT_EQ(GameSession::buildCM_REPLACE_ITEM(0, 0x01020304, 1, 0x05060708),
		(std::vector<uint8_t>{0x00, 0x04, 0x03, 0x02, 0x01, 0x01, 0x08, 0x07, 0x06, 0x05}));

	// CM_MANASTONE: readUC action, readUC fused slot, readD target, then the arm - the gate's godstone socketing is (4, 0, sword, stone, 0)
	EXPECT_EQ(GameSession::buildCM_MANASTONE(GameSession::MANASTONE_SOCKET_GODSTONE, 0, 0x0100, 0x0200),
		(std::vector<uint8_t>{0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
	for (const uint8_t action : std::initializer_list<uint8_t>{1, 2, 8})
		EXPECT_EQ(GameSession::buildCM_MANASTONE(action, 1, 3, 4, 5).size(), 14u) << "arm " << static_cast<int32_t>(action) << " reads two ints";
	GameSession::ManastoneRequest remove;
	remove.actionType = GameSession::MANASTONE_REMOVE;
	remove.targetFusedSlot = 1;
	remove.targetItemUniqueId = 0x0100;
	remove.slotNum = 5;
	remove.npcObjId = 0x0300;
	EXPECT_EQ(GameSession::buildCM_MANASTONE(remove),
		(std::vector<uint8_t>{0x03, 0x01, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00}))
		<< "arm 3: readUC slotNum, a dropped readC and readH, readD npcObjId";
	remove.actionType = 5;
	EXPECT_EQ(GameSession::buildCM_MANASTONE(remove).size(), 6u) << "an action without an arm reads nothing after the target";
	EXPECT_THROW(GameSession::buildCM_MANASTONE(GameSession::MANASTONE_REMOVE, 0, 1, 2), std::invalid_argument);

	// CM_EQUIP_ITEM: readC action, readQ slotRead, readD itemObjId
	PacketReader equip(GameSession::buildCM_EQUIP_ITEM(GameSession::EQUIP, 1, 0x0100));
	EXPECT_EQ(equip.C(), GameSession::EQUIP);
	EXPECT_EQ(equip.Q(), 1) << "MAIN_HAND, a long slot mask";
	EXPECT_EQ(equip.D(), 0x0100);
	EXPECT_EQ(equip.remaining(), 0u);
	EXPECT_EQ(GameSession::buildCM_EQUIP_ITEM(GameSession::UNEQUIP, 0, 7).size(), 13u);

	// CM_DELETE_ITEM: readD itemObjectId
	EXPECT_EQ(GameSession::buildCM_DELETE_ITEM(0x01020304), (std::vector<uint8_t>{0x04, 0x03, 0x02, 0x01}));
}

TEST(GameSessionTest, ServerPacketNames) {
	EXPECT_EQ(GameSession::nameOf(0), "SM_VERSION_CHECK");
	EXPECT_EQ(GameSession::nameOf(14), "SM_NPC_INFO");
	EXPECT_EQ(GameSession::nameOf(15), "SM_PLAYER_SPAWN");
	EXPECT_EQ(GameSession::nameOf(72), "SM_KEY");
	EXPECT_EQ(GameSession::nameOf(9), "SM_UNKNOWN_9");
	// the names castAndWait matches on (m5b2-plan.md G-02), and the four other packets of SkillDecoders.h
	EXPECT_EQ(GameSession::nameOf(33), "SM_CASTSPELL");
	EXPECT_EQ(GameSession::nameOf(42), "SM_SKILL_CANCEL");
	EXPECT_EQ(GameSession::nameOf(43), "SM_CASTSPELL_RESULT");
	EXPECT_EQ(GameSession::nameOf(49), "SM_ABNORMAL_STATE");
	EXPECT_EQ(GameSession::nameOf(50), "SM_ABNORMAL_EFFECT");
	EXPECT_EQ(GameSession::nameOf(51), "SM_SKILL_COOLDOWN");
	EXPECT_EQ(GameSession::nameOf(4), "SM_STATUPDATE_MP");
	// the names the M5b-3 gate matches its item and loot packets on (decoders/ItemDecoders.h; ServerPacketsOpcodes.java:45-47, 54, 148,
	// 187-189, 201, 223-224)
	EXPECT_EQ(GameSession::nameOf(27), "SM_INVENTORY_ADD_ITEM");
	EXPECT_EQ(GameSession::nameOf(28), "SM_DELETE_ITEM");
	EXPECT_EQ(GameSession::nameOf(29), "SM_INVENTORY_UPDATE_ITEM");
	EXPECT_EQ(GameSession::nameOf(36), "SM_UPDATE_PLAYER_APPEARANCE");
	EXPECT_EQ(GameSession::nameOf(130), "SM_CUBE_UPDATE");
	EXPECT_EQ(GameSession::nameOf(169), "SM_WAREHOUSE_ADD_ITEM");
	EXPECT_EQ(GameSession::nameOf(170), "SM_DELETE_WAREHOUSE_ITEM");
	EXPECT_EQ(GameSession::nameOf(171), "SM_WAREHOUSE_UPDATE_ITEM");
	EXPECT_EQ(GameSession::nameOf(183), "SM_ITEM_USAGE_ANIMATION");
	EXPECT_EQ(GameSession::nameOf(205), "SM_LOOT_STATUS");
	EXPECT_EQ(GameSession::nameOf(206), "SM_LOOT_ITEMLIST");
}

} // namespace
} // namespace aion::gameserver::scenario
