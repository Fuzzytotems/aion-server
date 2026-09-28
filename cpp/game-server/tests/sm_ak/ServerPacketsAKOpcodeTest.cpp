// P4-16: the opcodes of the server packets SM_A* to SM_K* against the numbers of ServerPacketsOpcodes.java (addPacketOpcode(opcode, SM_X.class),
// copied into the table below), and the recipients() kind of the packets whose Java writeImpl reads the connection (runtime-architecture.md
// §8.3; lint L10 checks the declarations, these tests the instances).

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string_view>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACCOUNT_PROPERTIES.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CREATE_CHARACTER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_PACKET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_REGISTRY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KEY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GAMEGUARD.h"
#include "SmAkTestSupport.h"

namespace aion::gameserver::network::aion::serverpackets::test {
namespace {

struct JavaOpcode {
	std::string_view name;
	int32_t javaOpcode;
	int32_t cppOpcode;
};

// ServerPacketsOpcodes.java, the addPacketOpcode lines of SM_A* to SM_K* (SM_CUSTOM_PACKET has no fixed opcode)
constexpr JavaOpcode JAVA_OPCODES[] = {
	{"SM_ABNORMAL_EFFECT", 50, opcodeOf<SM_ABNORMAL_EFFECT>},
	{"SM_ABNORMAL_STATE", 49, opcodeOf<SM_ABNORMAL_STATE>},
	{"SM_ABYSS_ARTIFACT_INFO3", 220, opcodeOf<SM_ABYSS_ARTIFACT_INFO3>},
	{"SM_ABYSS_RANK", 237, opcodeOf<SM_ABYSS_RANK>},
	{"SM_ABYSS_RANKING_LEGIONS", 139, opcodeOf<SM_ABYSS_RANKING_LEGIONS>},
	{"SM_ABYSS_RANKING_PLAYERS", 138, opcodeOf<SM_ABYSS_RANKING_PLAYERS>},
	{"SM_ABYSS_RANK_UPDATE", 136, opcodeOf<SM_ABYSS_RANK_UPDATE>},
	{"SM_ACCOUNT_PROPERTIES", 238, opcodeOf<SM_ACCOUNT_PROPERTIES>},
	{"SM_ACTION_ANIMATION", 70, opcodeOf<SM_ACTION_ANIMATION>},
	{"SM_AFTER_SIEGE_LOCINFO_475", 293, opcodeOf<SM_AFTER_SIEGE_LOCINFO_475>},
	{"SM_AFTER_TIME_CHECK_4_7_5", 292, opcodeOf<SM_AFTER_TIME_CHECK_4_7_5>},
	{"SM_ALLIANCE_INFO", 245, opcodeOf<SM_ALLIANCE_INFO>},
	{"SM_ALLIANCE_MEMBER_INFO", 246, opcodeOf<SM_ALLIANCE_MEMBER_INFO>},
	{"SM_ALLIANCE_READY_CHECK", 250, opcodeOf<SM_ALLIANCE_READY_CHECK>},
	{"SM_ASCENSION_MORPH", 182, opcodeOf<SM_ASCENSION_MORPH>},
	{"SM_ATREIAN_PASSPORT", 299, opcodeOf<SM_ATREIAN_PASSPORT>},
	{"SM_ATTACK", 54, opcodeOf<SM_ATTACK>},
	{"SM_ATTACK_RESPONSE", 115, opcodeOf<SM_ATTACK_RESPONSE>},
	{"SM_ATTACK_STATUS", 5, opcodeOf<SM_ATTACK_STATUS>},
	{"SM_AUTO_GROUP", 122, opcodeOf<SM_AUTO_GROUP>},
	{"SM_BIND_POINT_INFO", 235, opcodeOf<SM_BIND_POINT_INFO>},
	{"SM_BIND_POINT_TELEPORT", 296, opcodeOf<SM_BIND_POINT_TELEPORT>},
	{"SM_BLOCK_LIST", 224, opcodeOf<SM_BLOCK_LIST>},
	{"SM_BLOCK_RESPONSE", 223, opcodeOf<SM_BLOCK_RESPONSE>},
	{"SM_BROKER_SERVICE", 146, opcodeOf<SM_BROKER_SERVICE>},
	{"SM_CAPTCHA", 87, opcodeOf<SM_CAPTCHA>},
	{"SM_CASTSPELL", 33, opcodeOf<SM_CASTSPELL>},
	{"SM_CASTSPELL_RESULT", 43, opcodeOf<SM_CASTSPELL_RESULT>},
	{"SM_CHALLENGE_LIST", 280, opcodeOf<SM_CHALLENGE_LIST>},
	{"SM_CHANNEL_INFO", 229, opcodeOf<SM_CHANNEL_INFO>},
	{"SM_CHARACTER_LIST", 200, opcodeOf<SM_CHARACTER_LIST>},
	{"SM_CHARACTER_SELECT", 177, opcodeOf<SM_CHARACTER_SELECT>},
	{"SM_CHAT_INIT", 230, opcodeOf<SM_CHAT_INIT>},
	{"SM_CHAT_WINDOW", 99, opcodeOf<SM_CHAT_WINDOW>},
	{"SM_CLOSE_QUESTION_WINDOW", 53, opcodeOf<SM_CLOSE_QUESTION_WINDOW>},
	{"SM_CONQUEROR_PROTECTOR", 84, opcodeOf<SM_CONQUEROR_PROTECTOR>},
	{"SM_CRAFT_ANIMATION", 180, opcodeOf<SM_CRAFT_ANIMATION>},
	{"SM_CRAFT_UPDATE", 181, opcodeOf<SM_CRAFT_UPDATE>},
	{"SM_CREATE_CHARACTER", 201, opcodeOf<SM_CREATE_CHARACTER>},
	{"SM_CUBE_UPDATE", 130, opcodeOf<SM_CUBE_UPDATE>},
	{"SM_CUSTOM_SETTINGS", 184, opcodeOf<SM_CUSTOM_SETTINGS>},
	{"SM_DELETE", 22, opcodeOf<SM_DELETE>},
	{"SM_DELETE_CHARACTER", 202, opcodeOf<SM_DELETE_CHARACTER>},
	{"SM_DELETE_HOUSE", 272, opcodeOf<SM_DELETE_HOUSE>},
	{"SM_DELETE_HOUSE_OBJECT", 269, opcodeOf<SM_DELETE_HOUSE_OBJECT>},
	{"SM_DELETE_ITEM", 28, opcodeOf<SM_DELETE_ITEM>},
	{"SM_DELETE_WAREHOUSE_ITEM", 170, opcodeOf<SM_DELETE_WAREHOUSE_ITEM>},
	{"SM_DIALOG_WINDOW", 60, opcodeOf<SM_DIALOG_WINDOW>},
	{"SM_DIE", 193, opcodeOf<SM_DIE>},
	{"SM_DP_INFO", 7, opcodeOf<SM_DP_INFO>},
	{"SM_DUEL", 185, opcodeOf<SM_DUEL>},
	{"SM_EMOTION", 37, opcodeOf<SM_EMOTION>},
	{"SM_EMOTION_LIST", 79, opcodeOf<SM_EMOTION_LIST>},
	{"SM_ENTER_WORLD_CHECK", 13, opcodeOf<SM_ENTER_WORLD_CHECK>},
	{"SM_EXCHANGE_ADD_ITEM", 75, opcodeOf<SM_EXCHANGE_ADD_ITEM>},
	{"SM_EXCHANGE_ADD_KINAH", 77, opcodeOf<SM_EXCHANGE_ADD_KINAH>},
	{"SM_EXCHANGE_CONFIRMATION", 78, opcodeOf<SM_EXCHANGE_CONFIRMATION>},
	{"SM_EXCHANGE_REQUEST", 74, opcodeOf<SM_EXCHANGE_REQUEST>},
	{"SM_FIND_GROUP", 166, opcodeOf<SM_FIND_GROUP>},
	{"SM_FIRST_SHOW_DECOMPOSABLE", 284, opcodeOf<SM_FIRST_SHOW_DECOMPOSABLE>},
	{"SM_FLY_TIME", 244, opcodeOf<SM_FLY_TIME>},
	{"SM_FORCED_MOVE", 195, opcodeOf<SM_FORCED_MOVE>},
	{"SM_FORTRESS_INFO", 243, opcodeOf<SM_FORTRESS_INFO>},
	{"SM_FORTRESS_STATUS", 86, opcodeOf<SM_FORTRESS_STATUS>},
	{"SM_FRIEND_LIST", 132, opcodeOf<SM_FRIEND_LIST>},
	{"SM_FRIEND_NOTIFY", 225, opcodeOf<SM_FRIEND_NOTIFY>},
	{"SM_FRIEND_RESPONSE", 222, opcodeOf<SM_FRIEND_RESPONSE>},
	{"SM_FRIEND_STATUS", 227, opcodeOf<SM_FRIEND_STATUS>},
	{"SM_FRIEND_UPDATE", 240, opcodeOf<SM_FRIEND_UPDATE>},
	{"SM_GAMEGUARD", 125, opcodeOf<SM_GAMEGUARD>},
	{"SM_GAME_TIME", 38, opcodeOf<SM_GAME_TIME>},
	{"SM_GATHERABLE_INFO", 17, opcodeOf<SM_GATHERABLE_INFO>},
	{"SM_GATHER_ANIMATION", 34, opcodeOf<SM_GATHER_ANIMATION>},
	{"SM_GATHER_UPDATE", 35, opcodeOf<SM_GATHER_UPDATE>},
	{"SM_GF_WEBSHOP_TOKEN_RESPONSE", 274, opcodeOf<SM_GF_WEBSHOP_TOKEN_RESPONSE>},
	{"SM_GM_BOOKMARK_ADD", 64, opcodeOf<SM_GM_BOOKMARK_ADD>},
	{"SM_GM_SEARCH", 19, opcodeOf<SM_GM_SEARCH>},
	{"SM_GM_SHOW_LEGION_INFO", 63, opcodeOf<SM_GM_SHOW_LEGION_INFO>},
	{"SM_GM_SHOW_LEGION_MEMBERLIST", 66, opcodeOf<SM_GM_SHOW_LEGION_MEMBERLIST>},
	{"SM_GM_SHOW_PLAYER_SKILLS", 59, opcodeOf<SM_GM_SHOW_PLAYER_SKILLS>},
	{"SM_GM_SHOW_PLAYER_STATUS", 2, opcodeOf<SM_GM_SHOW_PLAYER_STATUS>},
	{"SM_GROUP_DATA_EXCHANGE", 178, opcodeOf<SM_GROUP_DATA_EXCHANGE>},
	{"SM_GROUP_INFO", 90, opcodeOf<SM_GROUP_INFO>},
	{"SM_GROUP_LOOT", 135, opcodeOf<SM_GROUP_LOOT>},
	{"SM_GROUP_MEMBER_INFO", 91, opcodeOf<SM_GROUP_MEMBER_INFO>},
	{"SM_HEADING_UPDATE", 57, opcodeOf<SM_HEADING_UPDATE>},
	{"SM_HOUSE_ACQUIRE", 275, opcodeOf<SM_HOUSE_ACQUIRE>},
	{"SM_HOUSE_BIDS", 256, opcodeOf<SM_HOUSE_BIDS>},
	{"SM_HOUSE_EDIT", 82, opcodeOf<SM_HOUSE_EDIT>},
	{"SM_HOUSE_OBJECT", 268, opcodeOf<SM_HOUSE_OBJECT>},
	{"SM_HOUSE_OBJECTS", 270, opcodeOf<SM_HOUSE_OBJECTS>},
	{"SM_HOUSE_OWNER_INFO", 263, opcodeOf<SM_HOUSE_OWNER_INFO>},
	{"SM_HOUSE_PAY_RENT", 262, opcodeOf<SM_HOUSE_PAY_RENT>},
	{"SM_HOUSE_REGISTRY", 116, opcodeOf<SM_HOUSE_REGISTRY>},
	{"SM_HOUSE_RENDER", 271, opcodeOf<SM_HOUSE_RENDER>},
	{"SM_HOUSE_SCRIPTS", 131, opcodeOf<SM_HOUSE_SCRIPTS>},
	{"SM_HOUSE_TELEPORT", 221, opcodeOf<SM_HOUSE_TELEPORT>},
	{"SM_HOUSE_UPDATE", 61, opcodeOf<SM_HOUSE_UPDATE>},
	{"SM_ICON_INFO", 175, opcodeOf<SM_ICON_INFO>},
	{"SM_INFLUENCE_RATIO", 85, opcodeOf<SM_INFLUENCE_RATIO>},
	{"SM_INSTANCE_COUNT_INFO", 147, opcodeOf<SM_INSTANCE_COUNT_INFO>},
	{"SM_INSTANCE_INFO", 141, opcodeOf<SM_INSTANCE_INFO>},
	{"SM_INSTANCE_SCORE", 121, opcodeOf<SM_INSTANCE_SCORE>},
	{"SM_INSTANCE_STAGE_INFO", 140, opcodeOf<SM_INSTANCE_STAGE_INFO>},
	{"SM_INVENTORY_ADD_ITEM", 27, opcodeOf<SM_INVENTORY_ADD_ITEM>},
	{"SM_INVENTORY_INFO", 26, opcodeOf<SM_INVENTORY_INFO>},
	{"SM_INVENTORY_UPDATE_ITEM", 29, opcodeOf<SM_INVENTORY_UPDATE_ITEM>},
	{"SM_ITEM_COOLDOWN", 103, opcodeOf<SM_ITEM_COOLDOWN>},
	{"SM_ITEM_USAGE_ANIMATION", 183, opcodeOf<SM_ITEM_USAGE_ANIMATION>},
	{"SM_KEY", 72, opcodeOf<SM_KEY>},
	{"SM_KISK_UPDATE", 144, opcodeOf<SM_KISK_UPDATE>},
};

TEST(ServerPacketsAKOpcodeTest, EveryPacketHasTheOpcodeOfServerPacketsOpcodesJava) {
	std::set<std::string_view> names;
	for (const JavaOpcode& entry : JAVA_OPCODES) {
		EXPECT_EQ(entry.cppOpcode, entry.javaOpcode) << entry.name;
		const ServerPacketsOpcodes::Entry* generated = ServerPacketsOpcodes::findByOpcode(entry.javaOpcode);
		ASSERT_NE(generated, nullptr) << entry.name;
		EXPECT_EQ(generated->name, entry.name);
		EXPECT_TRUE(names.insert(entry.name).second) << entry.name;
	}
	// the 111 Java files SM_A*.java to SM_K*.java with a fixed opcode, plus SM_CUSTOM_PACKET
	EXPECT_EQ(names.size(), 111u);
}

TEST_F(PacketTest, ConstructedPacketsCarryTheirOpcode) {
	EXPECT_EQ(SM_DELETE_HOUSE(1).getOpCode(), 272);
	EXPECT_EQ(SM_GAMEGUARD(0).getOpCode(), 125);
	EXPECT_EQ(SM_KEY().getOpCode(), 72);
	EXPECT_EQ(SM_FRIEND_RESPONSE::TARGET_OFFLINE->getOpCode(), opcodeOf<SM_FRIEND_RESPONSE>);
	// SM_CUSTOM_PACKET(opcode): the opcode given by //fsc
	EXPECT_EQ(SM_CUSTOM_PACKET(321).getOpCode(), 321);
}

TEST_F(PacketTest, PacketsReadingTheConnectionAreSerializedPerRecipient) {
	using Recipients = AionServerPacket::Recipients;
	EXPECT_EQ(SM_ACCOUNT_PROPERTIES().recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_BLOCK_LIST().recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_CHARACTER_LIST(0).recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_CREATE_CHARACTER(nullptr, SM_CREATE_CHARACTER::RESPONSE_DB_ERROR).recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_DIALOG_WINDOW(1, 2).recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_FRIEND_LIST().recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_FRIEND_UPDATE(1).recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_HOUSE_EDIT(1).recipients(), Recipients::PER_RECIPIENT);
	EXPECT_EQ(SM_HOUSE_REGISTRY(1).recipients(), Recipients::PER_RECIPIENT);
	// SM_KEY reads the connection but is serialized only by AionConnection::initialized for its own connection, never broadcast
	EXPECT_EQ(SM_KEY().recipients(), Recipients::SHARED);
	EXPECT_EQ(SM_DELETE_HOUSE(1).recipients(), Recipients::SHARED);
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::test
