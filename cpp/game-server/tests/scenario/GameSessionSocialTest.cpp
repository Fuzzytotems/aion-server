// The M5j stage-1 social builders of GameSession (m5j-plan.md §10.4, §18.1 CP1 "H-11's builders") read back in the order of the Java
// readImpl methods they serve: a field written with the wrong width or in the wrong order leaves bytes over or runs short.

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "GameSession.h"

#include "NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using network::test::PacketReader;

/** CM_FRIEND_ADD, CM_FRIEND_DEL, CM_BLOCK_ADD, CM_SET_NOTE: readS fields */
TEST(GameSessionSocialTest, TheStringPackets) {
	PacketReader add(GameSession::buildCM_FRIEND_ADD("Bravo", "be my friend"));
	EXPECT_EQ(add.S(), "Bravo");
	EXPECT_EQ(add.S(), "be my friend");
	EXPECT_EQ(add.remaining(), 0u);
	PacketReader del(GameSession::buildCM_FRIEND_DEL("Bravo"));
	EXPECT_EQ(del.S(), "Bravo");
	EXPECT_EQ(del.remaining(), 0u);
	PacketReader block(GameSession::buildCM_BLOCK_ADD("Bravo", "rude"));
	EXPECT_EQ(block.S(), "Bravo");
	EXPECT_EQ(block.S(), "rude");
	EXPECT_EQ(block.remaining(), 0u);
	PacketReader note(GameSession::buildCM_SET_NOTE("hello"));
	EXPECT_EQ(note.S(), "hello");
	EXPECT_EQ(note.remaining(), 0u);
}

/** CM_VIEW_PLAYER_DETAILS, CM_DUEL_REQUEST: readD; CM_TITLE_SET: readUH; CM_ABYSS_RANKING_PLAYERS: readC; CM_MACRO_CREATE: readUC, readS */
TEST(GameSessionSocialTest, TheNumberPackets) {
	PacketReader view(GameSession::buildCM_VIEW_PLAYER_DETAILS(123456));
	EXPECT_EQ(view.D(), 123456);
	EXPECT_EQ(view.remaining(), 0u);
	PacketReader duel(GameSession::buildCM_DUEL_REQUEST(654321));
	EXPECT_EQ(duel.D(), 654321);
	EXPECT_EQ(duel.remaining(), 0u);
	PacketReader title(GameSession::buildCM_TITLE_SET(0xFFFF));
	EXPECT_EQ(static_cast<uint16_t>(title.H()), 0xFFFF);
	EXPECT_EQ(title.remaining(), 0u);
	PacketReader ranking(GameSession::buildCM_ABYSS_RANKING_PLAYERS(1));
	EXPECT_EQ(ranking.C(), 1);
	EXPECT_EQ(ranking.remaining(), 0u);
	PacketReader macro(GameSession::buildCM_MACRO_CREATE(200, "<m/>"));
	EXPECT_EQ(macro.C(), 200);
	EXPECT_EQ(macro.S(), "<m/>");
	EXPECT_EQ(macro.remaining(), 0u);
}

/** CM_PLAYER_SEARCH.readImpl: readS(25) - the name, its NUL char and (25 - length) * 2 padding bytes - then readD, readD, readUC x3, readC */
TEST(GameSessionSocialTest, ThePlayerSearchNameIsFixedLength) {
	const std::vector<uint8_t> body = GameSession::buildCM_PLAYER_SEARCH("Bravo", 210010000, 0x40, 1, 55, 1);
	ASSERT_EQ(body.size(), (5u + 1u) * 2u + (25u - 5u) * 2u + 4u + 4u + 4u);
	PacketReader reader(body);
	EXPECT_EQ(reader.S(), "Bravo");
	const std::vector<uint8_t> padding = reader.B((25 - 5) * 2);
	EXPECT_EQ(padding, std::vector<uint8_t>(40, 0));
	EXPECT_EQ(reader.D(), 210010000);
	EXPECT_EQ(reader.D(), 0x40);
	EXPECT_EQ(reader.C(), 1);
	EXPECT_EQ(reader.C(), 55);
	EXPECT_EQ(reader.C(), 1);
	EXPECT_EQ(reader.C(), 0);
	EXPECT_EQ(reader.remaining(), 0u);
	const std::vector<uint8_t> any = GameSession::buildCM_PLAYER_SEARCH("");
	EXPECT_EQ(any.size(), 2u + 50u + 12u);
	EXPECT_EQ(any[any.size() - 4], 0xFF) << "minLevel 0xFF: any";
	EXPECT_THROW(GameSession::buildCM_PLAYER_SEARCH(std::string(26, 'a')), std::invalid_argument);
}

/** CM_PET.java:52-66 (M5j stage 2): ADOPT's eight fields, SPAWN's template id */
TEST(GameSessionSocialTest, ThePetPackets) {
	const std::vector<uint8_t> adoptBody = GameSession::buildCM_PET_ADOPT(5001, 900001, 7, "Kitty");
	PacketReader adopt(adoptBody);
	EXPECT_EQ(adopt.H(), GameSession::PET_ADOPT);
	EXPECT_EQ(adopt.D(), 5001);
	EXPECT_EQ(adopt.D(), 900001);
	EXPECT_EQ(adopt.C(), 0);
	EXPECT_EQ(adopt.D(), 0);
	EXPECT_EQ(adopt.D(), 7);
	EXPECT_EQ(adopt.D(), 0);
	EXPECT_EQ(adopt.D(), 0);
	EXPECT_EQ(adopt.S(), "Kitty");
	EXPECT_EQ(adopt.remaining(), 0u);
	const std::vector<uint8_t> spawnBody = GameSession::buildCM_PET(GameSession::PET_SPAWN, 900001);
	PacketReader spawn(spawnBody);
	EXPECT_EQ(spawn.H(), 3);
	EXPECT_EQ(spawn.D(), 900001);
	EXPECT_EQ(spawn.remaining(), 0u);
}

} // namespace
} // namespace aion::gameserver::scenario
