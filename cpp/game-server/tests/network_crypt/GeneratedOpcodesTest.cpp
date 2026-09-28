// Compiles and checks the generated network/aion/ServerPacketsOpcodes.gen.h and ClientPacketInfo.gen.inc (cpp/tools/gen/opcodes.py) and
// cross-checks their obfuscated wire opcodes (computed by the generator in Python) against Crypt.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "GameClientCrypto.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

// Every client packet class name expands to a valid forward declaration
namespace aion::gameserver::network::aion::clientpackets {
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) class Class;
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::network::aion {
namespace {

enum StateBits : uint8_t { CONNECTED = 1, AUTHED = 2, IN_GAME = 4 };

struct ClientPacketInfo {
	int32_t opcode;
	uint16_t wireOpcode;
	std::string_view name;
	std::string_view clientName;
	uint8_t states;
};

constexpr uint8_t stateMask(std::initializer_list<StateBits> states) {
	uint8_t mask = 0;
	for (StateBits s : states)
		mask |= s;
	return mask;
}

#define AION_CLIENT_PACKET_TABLE_SIZE(size) constexpr int32_t CLIENT_PACKET_TABLE_SIZE = size;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
#undef AION_CLIENT_PACKET_TABLE_SIZE

const std::vector<ClientPacketInfo> CLIENT_PACKETS = {
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) {opcode, wireOpcode, #Class, clientName, stateMask({__VA_ARGS__})},
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
};

// --- server opcodes -------------------------------------------------------------------------------------------------------------------------

static_assert(opcodeOf<serverpackets::SM_VERSION_CHECK> == 0);
static_assert(opcodeOf<serverpackets::SM_SYSTEM_MESSAGE> == 25);
static_assert(opcodeOf<serverpackets::SM_KEY> == 72);
static_assert(opcodeOf<serverpackets::SM_LEGION_DOMINION_LOC_INFO> == 303);
static_assert(ServerPacketsOpcodes::INTERNAL_VERSION == Crypt::INTERNAL_VERSION);
static_assert(ServerPacketsOpcodes::findByOpcode(72) != nullptr && ServerPacketsOpcodes::findByOpcode(72)->name == "SM_KEY");
static_assert(ServerPacketsOpcodes::findByOpcode(9) == nullptr); // commented out in Java ([S_LOGIN_CHECK])

TEST(GeneratedOpcodesTest, ServerEntriesAreSortedUniqueAndMatchTheConstants) {
	const auto& entries = ServerPacketsOpcodes::ENTRIES;
	EXPECT_EQ(entries.size(), 237u);
	std::set<std::string_view> names;
	for (size_t i = 0; i < entries.size(); i++) {
		if (i > 0)
			ASSERT_LT(entries[i - 1].opcode, entries[i].opcode);
		ASSERT_TRUE(names.insert(entries[i].name).second) << entries[i].name;
		ASSERT_EQ(ServerPacketsOpcodes::findByOpcode(entries[i].opcode), &entries[i]);
		ASSERT_TRUE(entries[i].name.starts_with("SM_"));
	}
	EXPECT_EQ(entries.back().opcode, 303);
	EXPECT_EQ(ServerPacketsOpcodes::findByOpcode(72)->clientName, "S_KEY");
	EXPECT_EQ(ServerPacketsOpcodes::findByOpcode(99)->clientName, "S_ASK_INFO_RESULT"); // Java comment: [S_ASK_INFO_RESULT] 2.1
	EXPECT_EQ(ServerPacketsOpcodes::findByOpcode(-1), nullptr);
	EXPECT_EQ(ServerPacketsOpcodes::findByOpcode(304), nullptr);
}

TEST(GeneratedOpcodesTest, ServerWireOpcodesMatchCrypt) {
	for (const auto& e : ServerPacketsOpcodes::ENTRIES) {
		const int32_t encoded = Crypt::encodeServerPacketOpcode(e.opcode);
		ASSERT_GE(encoded, 0);
		ASSERT_LE(encoded, 0xFFFF);
		ASSERT_EQ(e.wireOpcode, static_cast<uint16_t>(encoded)) << e.name;
		ASSERT_EQ(e.wireOpcode, test::GameClientCrypto::serverWireOpcode(e.opcode)) << e.name;
	}
	EXPECT_EQ(ServerPacketsOpcodes::findByOpcode(72)->wireOpcode, 0x01C8); // (72 + 207) ^ 0xDF
}

// --- client packet info ---------------------------------------------------------------------------------------------------------------------

TEST(GeneratedOpcodesTest, ClientPacketsAreSortedUniqueAndInsideTheJavaTable) {
	EXPECT_EQ(CLIENT_PACKET_TABLE_SIZE, 250);
	EXPECT_EQ(CLIENT_PACKETS.size(), 186u);
	std::set<std::string_view> names;
	for (size_t i = 0; i < CLIENT_PACKETS.size(); i++) {
		const auto& p = CLIENT_PACKETS[i];
		if (i > 0)
			ASSERT_LT(CLIENT_PACKETS[i - 1].opcode, p.opcode);
		ASSERT_GE(p.opcode, 0);
		ASSERT_LT(p.opcode, CLIENT_PACKET_TABLE_SIZE);
		ASSERT_TRUE(names.insert(p.name).second) << p.name;
		ASSERT_TRUE(p.name.starts_with("CM_"));
		ASSERT_NE(p.states, 0);
	}
}

TEST(GeneratedOpcodesTest, ClientWireOpcodesDecodeWithCrypt) {
	for (const auto& p : CLIENT_PACKETS) {
		ASSERT_EQ(Crypt::decodeClientPacketOpcode(p.wireOpcode), p.opcode) << p.name;
		ASSERT_EQ(p.wireOpcode, test::GameClientCrypto::clientWireOpcode(p.opcode)) << p.name;
	}
	const auto versionCheck = std::ranges::find(CLIENT_PACKETS, std::string_view("CM_VERSION_CHECK"), &ClientPacketInfo::name);
	ASSERT_NE(versionCheck, CLIENT_PACKETS.end());
	EXPECT_EQ(versionCheck->opcode, 0);
	EXPECT_EQ(versionCheck->wireOpcode, 0x00C3);
	EXPECT_EQ(versionCheck->clientName, "C_VERSION (VersionPacket)");
}

TEST(GeneratedOpcodesTest, ClientAllowedStatesMatchTheJavaFactory) {
	std::map<std::string_view, const ClientPacketInfo*> byName;
	std::map<uint8_t, int> combinations;
	for (const auto& p : CLIENT_PACKETS) {
		byName[p.name] = &p;
		combinations[p.states]++;
	}
	EXPECT_EQ(byName.at("CM_VERSION_CHECK")->states, CONNECTED);
	EXPECT_EQ(byName.at("CM_L2AUTH_LOGIN_CHECK")->states, CONNECTED);
	EXPECT_EQ(byName.at("CM_MAC_ADDRESS")->states, CONNECTED);
	EXPECT_EQ(byName.at("CM_TIME_CHECK")->states, CONNECTED | AUTHED | IN_GAME);
	EXPECT_EQ(byName.at("CM_SECURITY_TOKEN")->states, CONNECTED | AUTHED | IN_GAME);
	EXPECT_EQ(byName.at("CM_MOVE")->states, IN_GAME);
	EXPECT_EQ(byName.at("CM_MOVE")->opcode, 48);
	EXPECT_EQ(byName.at("CM_CASTSPELL")->opcode, 33);
	EXPECT_EQ(byName.at("CM_DISCONNECT")->states, AUTHED | IN_GAME);
	EXPECT_EQ(combinations[IN_GAME], 167);
	EXPECT_EQ(combinations[AUTHED], 10);
	EXPECT_EQ(combinations[AUTHED | IN_GAME], 4);
	EXPECT_EQ(combinations[CONNECTED], 3);
	EXPECT_EQ(combinations[CONNECTED | AUTHED | IN_GAME], 2);
	EXPECT_FALSE(byName.contains("CM_GODSTONE_SOCKET")); // commented out in Java
	EXPECT_FALSE(byName.contains("CM_TIME_CHECK_QUIT"));
}

} // namespace
} // namespace aion::gameserver::network::aion
