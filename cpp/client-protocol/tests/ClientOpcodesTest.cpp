#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "aion/clientprotocol/ClientOpcodes.gen.h"
#include "aion/clientprotocol/ClientPackets.h"
#include "aion/clientprotocol/GameClientCrypto.h"

// The generated client-side opcode table against the codec of GameClientCrypto and the opcodes the builders of ClientPackets are written for.

namespace aion::clientprotocol {
namespace {

TEST(ClientOpcodesTest, WireOpcodesMatchTheCodec) {
	static_assert(ClientOpcodes::INTERNAL_VERSION == GameClientCrypto::INTERNAL_VERSION);
	for (const auto& entry : ClientOpcodes::CLIENT_PACKETS) {
		EXPECT_EQ(entry.wireOpcode, GameClientCrypto::clientWireOpcode(entry.opcode)) << entry.name;
		EXPECT_EQ(entry.direction, PacketDirection::CLIENT_TO_SERVER) << entry.name;
		EXPECT_EQ(std::string(entry.name).substr(0, 3), "CM_");
	}
	for (const auto& entry : ClientOpcodes::SERVER_PACKETS) {
		EXPECT_EQ(entry.wireOpcode, GameClientCrypto::serverWireOpcode(entry.opcode)) << entry.name;
		EXPECT_EQ(entry.direction, PacketDirection::SERVER_TO_CLIENT) << entry.name;
		EXPECT_EQ(std::string(entry.name).substr(0, 3), "SM_");
	}
}

TEST(ClientOpcodesTest, Lookups) {
	static_assert(ClientOpcodes::SM_KEY == GameClientCrypto::SM_KEY_OPCODE);
	static_assert(ClientOpcodes::findServerPacket(ClientOpcodes::SM_KEY)->name == "SM_KEY");
	static_assert(ClientOpcodes::findClientPacket(ClientOpcodes::CM_VERSION_CHECK)->clientName == "C_VERSION (VersionPacket)");
	EXPECT_EQ(ClientOpcodes::findClientPacket(1), nullptr); // no packets[1]
	EXPECT_EQ(ClientOpcodes::findServerPacket(-1), nullptr);
	EXPECT_EQ(ClientOpcodes::findServerPacketByWireOpcode(GameClientCrypto::serverWireOpcode(ClientOpcodes::SM_KEY))->opcode, ClientOpcodes::SM_KEY);
	EXPECT_EQ(ClientOpcodes::findClientPacketByWireOpcode(GameClientCrypto::clientWireOpcode(ClientOpcodes::CM_PING))->name, "CM_PING");
	for (size_t i = 1; i < ClientOpcodes::SERVER_PACKETS.size(); i++)
		EXPECT_LT(ClientOpcodes::SERVER_PACKETS[i - 1].opcode, ClientOpcodes::SERVER_PACKETS[i].opcode);
	for (size_t i = 1; i < ClientOpcodes::CLIENT_PACKETS.size(); i++)
		EXPECT_LT(ClientOpcodes::CLIENT_PACKETS[i - 1].opcode, ClientOpcodes::CLIENT_PACKETS[i].opcode);
}

TEST(ClientOpcodesTest, BuilderOpcodesAreTheGeneratedOnes) {
	// ClientPackets writes its opcodes out from AionClientPacketFactory by hand (the scenario gates keep the CM side independent of the
	// generator); this keeps the two in step
#define AION_EXPECT_SAME(name) EXPECT_EQ(ClientPackets::name, ClientOpcodes::name) << #name
	AION_EXPECT_SAME(CM_VERSION_CHECK);
	AION_EXPECT_SAME(CM_QUIT);
	AION_EXPECT_SAME(CM_ENTER_WORLD);
	AION_EXPECT_SAME(CM_LEVEL_READY);
	AION_EXPECT_SAME(CM_TIME_CHECK);
	AION_EXPECT_SAME(CM_PING);
	AION_EXPECT_SAME(CM_MOVE);
	AION_EXPECT_SAME(CM_SECURITY_TOKEN);
	AION_EXPECT_SAME(CM_GAMEGUARD);
	AION_EXPECT_SAME(CM_L2AUTH_LOGIN_CHECK);
	AION_EXPECT_SAME(CM_CHARACTER_LIST);
	AION_EXPECT_SAME(CM_CREATE_CHARACTER);
	AION_EXPECT_SAME(CM_CHECK_NICKNAME);
	AION_EXPECT_SAME(CM_MAY_LOGIN_INTO_GAME);
	AION_EXPECT_SAME(CM_MAC_ADDRESS);
	AION_EXPECT_SAME(CM_CUSTOM_SETTINGS);
	AION_EXPECT_SAME(CM_SUBZONE_CHANGE);
	AION_EXPECT_SAME(CM_REVIVE);
	AION_EXPECT_SAME(CM_TARGET_SELECT);
	AION_EXPECT_SAME(CM_ATTACK);
	AION_EXPECT_SAME(CM_CASTSPELL);
	AION_EXPECT_SAME(CM_REMOVE_ALTERED_STATE);
	AION_EXPECT_SAME(CM_USE_ITEM);
	AION_EXPECT_SAME(CM_EQUIP_ITEM);
	AION_EXPECT_SAME(CM_MANASTONE);
	AION_EXPECT_SAME(CM_DELETE_ITEM);
	AION_EXPECT_SAME(CM_START_LOOT);
	AION_EXPECT_SAME(CM_LOOT_ITEM);
	AION_EXPECT_SAME(CM_MOVE_ITEM);
	AION_EXPECT_SAME(CM_SPLIT_ITEM);
	AION_EXPECT_SAME(CM_REPLACE_ITEM);
	AION_EXPECT_SAME(CM_QUESTION_RESPONSE);
	AION_EXPECT_SAME(CM_SHOW_DIALOG);
	AION_EXPECT_SAME(CM_CLOSE_DIALOG);
	AION_EXPECT_SAME(CM_DIALOG_SELECT);
	AION_EXPECT_SAME(CM_DELETE_QUEST);
	AION_EXPECT_SAME(CM_PLAY_MOVIE_END);
	AION_EXPECT_SAME(CM_BUY_ITEM);
	AION_EXPECT_SAME(CM_EXCHANGE_REQUEST);
	AION_EXPECT_SAME(CM_EXCHANGE_ADD_ITEM);
	AION_EXPECT_SAME(CM_EXCHANGE_ADD_KINAH);
	AION_EXPECT_SAME(CM_EXCHANGE_LOCK);
	AION_EXPECT_SAME(CM_EXCHANGE_OK);
	AION_EXPECT_SAME(CM_EXCHANGE_CANCEL);
	AION_EXPECT_SAME(CM_RECIPE_DELETE);
	AION_EXPECT_SAME(CM_PRIVATE_STORE);
	AION_EXPECT_SAME(CM_PRIVATE_STORE_NAME);
	AION_EXPECT_SAME(CM_SEND_MAIL);
	AION_EXPECT_SAME(CM_CHECK_MAIL_LIST);
	AION_EXPECT_SAME(CM_READ_MAIL);
	AION_EXPECT_SAME(CM_GET_MAIL_ATTACHMENT);
	AION_EXPECT_SAME(CM_DELETE_MAIL);
	AION_EXPECT_SAME(CM_CRAFT);
	AION_EXPECT_SAME(CM_TOGGLE_SKILL_DEACTIVATE);
	AION_EXPECT_SAME(CM_SUMMON_COMMAND);
	AION_EXPECT_SAME(CM_SUMMON_ATTACK);
	AION_EXPECT_SAME(CM_USE_CHARGE_SKILL);
	AION_EXPECT_SAME(CM_TUNE);
	AION_EXPECT_SAME(CM_SELECT_DECOMPOSABLE);
	AION_EXPECT_SAME(CM_TUNE_RESULT);
	AION_EXPECT_SAME(CM_TELEPORT_ANIMATION_DONE);
	AION_EXPECT_SAME(CM_EMOTION);
	AION_EXPECT_SAME(CM_INSTANCE_LEAVE);
	AION_EXPECT_SAME(CM_MOVE_IN_AIR);
	AION_EXPECT_SAME(CM_TELEPORT_SELECT);
	AION_EXPECT_SAME(CM_BIND_POINT_TELEPORT);
	AION_EXPECT_SAME(CM_FIND_GROUP);
	AION_EXPECT_SAME(CM_GROUP_DATA_EXCHANGE);
	AION_EXPECT_SAME(CM_PLAYER_STATUS_INFO);
	AION_EXPECT_SAME(CM_INVITE_TO_GROUP);
	AION_EXPECT_SAME(CM_GROUP_DISTRIBUTION);
	AION_EXPECT_SAME(CM_QUEST_SHARE);
	AION_EXPECT_SAME(CM_SHOW_BRAND);
	AION_EXPECT_SAME(CM_GROUP_LOOT);
	AION_EXPECT_SAME(CM_DISTRIBUTION_SETTINGS);
	AION_EXPECT_SAME(CM_CHAT_MESSAGE_PUBLIC);
	AION_EXPECT_SAME(CM_CHAT_MESSAGE_WHISPER);
	AION_EXPECT_SAME(CM_BUILDER_COMMAND);
	AION_EXPECT_SAME(CM_SET_NOTE);
	AION_EXPECT_SAME(CM_VIEW_PLAYER_DETAILS);
	AION_EXPECT_SAME(CM_FRIEND_ADD);
	AION_EXPECT_SAME(CM_FRIEND_DEL);
	AION_EXPECT_SAME(CM_DUEL_REQUEST);
	AION_EXPECT_SAME(CM_TITLE_SET);
	AION_EXPECT_SAME(CM_PLAYER_SEARCH);
	AION_EXPECT_SAME(CM_BLOCK_ADD);
	AION_EXPECT_SAME(CM_MACRO_CREATE);
	AION_EXPECT_SAME(CM_ABYSS_RANKING_PLAYERS);
	AION_EXPECT_SAME(CM_PET);
	AION_EXPECT_SAME(CM_AUTO_GROUP);
#undef AION_EXPECT_SAME
}

TEST(GameClientCryptoTest, ClientFrameLayout) {
	GameClientCrypto crypto;
	crypto.setBaseKey(0x12345678);
	const uint64_t key = GameClientCrypto::KEY_HIGH_WORD | 0x12345678u;
	const std::vector<uint8_t> data = {1, 2, 3};
	std::vector<uint8_t> frame = crypto.buildClientFrame(ClientOpcodes::CM_PING, data);
	ASSERT_EQ(frame.size(), 10u);
	EXPECT_EQ(frame[0], 10);
	EXPECT_EQ(frame[1], 0);
	// decrypting = encrypting's inverse: c[i] = b[i] ^ mask(i, c[i - 1]); undo it with the same mask from the cipher bytes
	std::vector<uint8_t> plain(frame.begin() + 2, frame.end());
	uint8_t previous = 0;
	for (size_t i = 0; i < plain.size(); i++) {
		const uint8_t cipher = plain[i];
		uint8_t mask = static_cast<uint8_t>(key >> (8 * (i % 8)));
		if (i > 0)
			mask ^= static_cast<uint8_t>(GameClientCrypto::STATIC_KEY[i % 64]) ^ previous;
		plain[i] = cipher ^ mask;
		previous = cipher;
	}
	const uint16_t wire = GameClientCrypto::clientWireOpcode(ClientOpcodes::CM_PING);
	EXPECT_EQ(plain, (std::vector<uint8_t>{static_cast<uint8_t>(wire), static_cast<uint8_t>(wire >> 8), 0x65, static_cast<uint8_t>(~wire),
		static_cast<uint8_t>(~wire >> 8), 1, 2, 3}));
}

TEST(GameClientCryptoTest, ParseServerFrame) {
	const uint16_t wire = GameClientCrypto::serverWireOpcode(ClientOpcodes::SM_KEY);
	const std::vector<uint8_t> frame = {11, 0, static_cast<uint8_t>(wire), static_cast<uint8_t>(wire >> 8), 0x44, static_cast<uint8_t>(~wire),
		static_cast<uint8_t>(~wire >> 8), 4, 3, 2, 1};
	const ServerPacket packet = GameClientCrypto::parseServerFrame(frame);
	EXPECT_EQ(packet.opcode, ClientOpcodes::SM_KEY);
	EXPECT_EQ(packet.data, (std::vector<uint8_t>{4, 3, 2, 1}));
	std::vector<uint8_t> broken = frame;
	broken[4] = 0x65;
	EXPECT_THROW(GameClientCrypto::parseServerFrame(broken), std::runtime_error);
	EXPECT_THROW(GameClientCrypto::parseServerFrame(std::vector<uint8_t>(6)), std::runtime_error);
}

} // namespace
} // namespace aion::clientprotocol
