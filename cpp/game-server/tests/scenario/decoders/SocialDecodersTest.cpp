// The M5j stage-1 social decoders (m5j-plan.md §10.4, §18.1 CP1) against bodies written out field by field in the Java writeImpl order
// (m5a-plan.md D9): every case also relies on the decoders' exact-consumption check, so a field read with the wrong width or in the wrong order
// fails. No case includes or consults a C++ serverpackets header.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "decoders/SocialDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** a writeS(text, fixedLength) of AionServerPacket.java:90-98 */
void fixedS(PacketWriter& w, std::u16string_view text, size_t fixedLength) {
	for (size_t i = 0; i < fixedLength; i++)
		w.H(i < text.size() ? text[i] : 0);
	w.H(0);
}

/** SM_FRIEND_RESPONSE.java writeImpl, SM_FRIEND_NOTIFY.java writeImpl */
TEST(SocialDecodersTest, FriendResponseAndNotify) {
	const FriendResponse added = decodeFriendResponse(PacketWriter().S("Bravo").C(FRIEND_RESPONSE_TARGET_ADDED).data);
	EXPECT_EQ(added.name, "Bravo");
	EXPECT_EQ(added.code, FRIEND_RESPONSE_TARGET_ADDED);
	EXPECT_EQ(decodeFriendResponse(PacketWriter().S("").C(3).data).code, FRIEND_RESPONSE_TARGET_NOT_FOUND);
	EXPECT_THROW(decodeFriendResponse(PacketWriter().S("x").C(0).C(0).data), DecodeError);
	const FriendNotify notify = decodeFriendNotify(PacketWriter().S("Alpha").C(FRIEND_NOTIFY_LOGOUT).data);
	EXPECT_EQ(notify.name, "Alpha");
	EXPECT_EQ(notify.code, FRIEND_NOTIFY_LOGOUT);
}

/** SM_FRIEND_LIST.java writeImpl: writeH(-size), writeC(0), the twelve fields per friend */
TEST(SocialDecodersTest, FriendList) {
	PacketWriter w;
	w.H(-2).C(0);
	w.D(101).S("Alpha").D(10).D(1).C(0).D(210010000).D(0).S("note a").C(FRIEND_STATUS_ONLINE).D(0).C(0).S("");
	w.D(102).S("Bravo").D(9).D(0).C(1).D(210010000).D(1700000000).S("").C(FRIEND_STATUS_OFFLINE).D(0).C(0).S("memo");
	const FriendList list = decodeFriendList(w.data);
	ASSERT_EQ(list.friends.size(), 2u);
	ASSERT_NE(list.find(102), nullptr);
	EXPECT_EQ(list.find(101)->note, "note a");
	EXPECT_EQ(list.find(102)->memo, "memo");
	EXPECT_EQ(list.find(102)->lastOnline, 1700000000);
	EXPECT_EQ(list.find(103), nullptr);
	EXPECT_TRUE(decodeFriendList(PacketWriter().H(0).C(0).data).friends.empty());
	EXPECT_THROW(decodeFriendList(PacketWriter().H(0).C(1).data), DecodeError) << "the literal writeC(0)";
}

/** SM_FRIEND_UPDATE.java writeImpl: the friend's fields, or nothing */
TEST(SocialDecodersTest, FriendUpdate) {
	const FriendUpdate update = decodeFriendUpdate(PacketWriter().S("Bravo").D(10).D(7).C(1).D(210010000).D(0).S("hi").C(1).data);
	EXPECT_FALSE(update.empty);
	EXPECT_EQ(update.name, "Bravo");
	EXPECT_EQ(update.classId, 7);
	EXPECT_EQ(update.note, "hi");
	EXPECT_EQ(update.status, FRIEND_STATUS_ONLINE);
	EXPECT_TRUE(decodeFriendUpdate({}).empty);
}

/** SM_BLOCK_RESPONSE.java, SM_BLOCK_LIST.java writeImpl */
TEST(SocialDecodersTest, BlockResponseAndList) {
	const BlockResponse response = decodeBlockResponse(PacketWriter().S("Bravo").C(BLOCK_RESPONSE_BLOCK_SUCCESSFUL).data);
	EXPECT_EQ(response.name, "Bravo");
	EXPECT_EQ(response.code, BLOCK_RESPONSE_BLOCK_SUCCESSFUL);
	const BlockList list = decodeBlockList(PacketWriter().H(-1).C(0).S("Bravo").S("rude").data);
	ASSERT_EQ(list.blocked.size(), 1u);
	EXPECT_EQ(list.blocked[0], (std::pair<std::string, std::string>{"Bravo", "rude"}));
}

/** SM_UPDATE_NOTE.java writeImpl */
TEST(SocialDecodersTest, UpdateNote) {
	const UpdateNote note = decodeUpdateNote(PacketWriter().D(55).S("a note").data);
	EXPECT_EQ(note.objectId, 55);
	EXPECT_EQ(note.note, "a note");
	EXPECT_THROW(decodeUpdateNote(PacketWriter().D(55).data), DecodeError);
}

/** SM_TITLE_INFO.java writeImpl: the list, self set and broad set arms */
TEST(SocialDecodersTest, TitleInfo) {
	const TitleInfo list = decodeTitleInfo(PacketWriter().C(0).C(0).H(2).D(1).D(0).D(5).D(3600).data);
	EXPECT_EQ(list.action, TITLE_ACTION_LIST);
	EXPECT_EQ(list.titles, (std::vector<std::pair<int32_t, int32_t>>{{1, 0}, {5, 3600}}));
	const TitleInfo self = decodeTitleInfo(PacketWriter().C(1).H(0xFFFF).data);
	EXPECT_EQ(self.action, TITLE_ACTION_SELF_SET);
	EXPECT_EQ(self.titleId, 0xFFFF);
	const TitleInfo broad = decodeTitleInfo(PacketWriter().C(3).D(55).H(1).data);
	EXPECT_EQ(broad.playerObjectId, 55);
	EXPECT_EQ(broad.titleId, 1);
	EXPECT_THROW(decodeTitleInfo(PacketWriter().C(1).D(1).data), DecodeError);
}

/** SM_PLAYER_SEARCH.java writeImpl, the name written with writeS(text, CHARNAME_MAX_LENGTH + 2) */
TEST(SocialDecodersTest, PlayerSearch) {
	PacketWriter w;
	w.H(1).D(210010000).F(1.5f).F(2.5f).F(3.5f).C(7).C(0).C(10).C(0);
	fixedS(w, u"Bravo", CHARNAME_MAX_LENGTH + 2);
	const std::vector<PlayerSearchEntry> entries = decodePlayerSearch(w.data);
	ASSERT_EQ(entries.size(), 1u);
	EXPECT_EQ(entries[0].worldId, 210010000);
	EXPECT_FLOAT_EQ(entries[0].y, 2.5f);
	EXPECT_EQ(entries[0].level, 10);
	EXPECT_EQ(entries[0].name, "Bravo");
	EXPECT_TRUE(decodePlayerSearch(PacketWriter().H(0).data).empty());
	PacketWriter shortName;
	shortName.H(1).D(1).F(0).F(0).F(0).C(0).C(0).C(1).C(0).S("Bravo");
	EXPECT_THROW(decodePlayerSearch(shortName.data), DecodeError) << "the name is fixed length";
}

/** SM_VIEW_PLAYER_DETAILS.java writeImpl's head */
TEST(SocialDecodersTest, ViewPlayerDetailsHead) {
	const ViewPlayerDetailsHead empty = decodeViewPlayerDetailsHead(PacketWriter().D(55).C(11).H(0).data);
	EXPECT_EQ(empty.targetObjectId, 55);
	EXPECT_EQ(empty.itemCount, 0);
	EXPECT_EQ(decodeViewPlayerDetailsHead(PacketWriter().D(55).C(11).H(3).zeros(10).data).itemCount, 3);
	EXPECT_THROW(decodeViewPlayerDetailsHead(PacketWriter().D(55).C(10).H(0).data), DecodeError) << "the literal writeC(11)";
	EXPECT_THROW(decodeViewPlayerDetailsHead(PacketWriter().D(55).C(11).H(2).data), DecodeError) << "a count without items";
}

/** SM_DUEL.java writeImpl: started, result, and a type Java never writes */
TEST(SocialDecodersTest, Duel) {
	const Duel started = decodeDuel(PacketWriter().C(0).D(77).data);
	EXPECT_EQ(started.type, DUEL_TYPE_STARTED);
	EXPECT_EQ(started.requesterObjectId, 77);
	const Duel lost = decodeDuel(PacketWriter().C(1).C(DUEL_RESULT_LOST).D(DUEL_LOST_MESSAGE).S("Bravo").data);
	EXPECT_EQ(lost.type, DUEL_TYPE_RESULT);
	EXPECT_EQ(lost.resultId, DUEL_RESULT_LOST);
	EXPECT_EQ(lost.messageId, DUEL_LOST_MESSAGE);
	EXPECT_EQ(lost.name, "Bravo");
	EXPECT_EQ(decodeDuel(PacketWriter().C(0xE0).data).type, 0xE0);
	EXPECT_THROW(decodeDuel(PacketWriter().C(2).data), DecodeError);
}

/** SM_ABYSS_RANK.java writeImpl */
TEST(SocialDecodersTest, AbyssRank) {
	PacketWriter w;
	w.Q(923).D(0).D(1).D(0).D(0).D(1).D(1).D(0).Q(0).D(0).D(0).Q(0).D(0).D(0).Q(0).D(0).C(0);
	const AbyssRank rank = decodeAbyssRank(w.data);
	EXPECT_EQ(rank.ap, 923);
	EXPECT_EQ(rank.rankId, 1);
	EXPECT_EQ(rank.allKill, 1);
	PacketWriter bad;
	bad.Q(1).D(0).D(1).D(0).D(5).D(0).D(1).D(0).Q(0).D(0).D(0).Q(0).D(0).D(0).Q(0).D(0).C(0);
	EXPECT_THROW(decodeAbyssRank(bad.data), DecodeError) << "the literal writeD(0)";
}

/** SM_ABYSS_RANKING_PLAYERS.java writeImpl: the short answer (no rows) and one row */
TEST(SocialDecodersTest, AbyssRankingPlayers) {
	const AbyssRankingPlayers shortAnswer = decodeAbyssRankingPlayers(PacketWriter().D(0).D(1700000000).D(0).D(0).H(0).data);
	EXPECT_EQ(shortAnswer.lastUpdate, 1700000000);
	EXPECT_TRUE(shortAnswer.players.empty());
	PacketWriter w;
	w.D(1).D(5).D(1).D(0x7F).H(1).D(1).D(10).D(2).D(99).D(1).D(7).C(1).C(0).C(0).C(0).Q(150000).D(1244).H(55);
	fixedS(w, u"Charlie", CHARNAME_MAX_LENGTH);
	fixedS(w, u"", 42);
	const AbyssRankingPlayers page = decodeAbyssRankingPlayers(w.data);
	ASSERT_EQ(page.players.size(), 1u);
	EXPECT_EQ(page.players[0].objectId, 99);
	EXPECT_EQ(page.players[0].ap, 150000);
	EXPECT_EQ(page.players[0].name, "Charlie");
	EXPECT_THROW(decodeAbyssRankingPlayers(PacketWriter().D(0).D(1).D(0).D(1).H(0).data), DecodeError) << "the end flag";
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
