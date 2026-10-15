#pragma once

// The social and abyss packets the M5j stage-1 gate compares the server against (m5j-plan.md §10.4, §18.1 CP1 "SocialDecoders"): the friend
// list and its answers (SM_FRIEND_RESPONSE, SM_FRIEND_LIST, SM_FRIEND_UPDATE, SM_FRIEND_NOTIFY), the block list (SM_BLOCK_RESPONSE,
// SM_BLOCK_LIST), the note and titles (SM_UPDATE_NOTE, SM_TITLE_INFO; SM_MACRO_LIST is PacketDecoders' V10 decoder), the two lookups (SM_PLAYER_SEARCH,
// SM_VIEW_PLAYER_DETAILS' head), the duel (SM_DUEL) and the abyss rank (SM_ABYSS_RANK, SM_ABYSS_RANKING_PLAYERS).
//
// **m5a-plan.md D9:** every layout below is written from the Java `writeImpl` under
// game-server/src/com/aionemu/gameserver/network/aion/serverpackets/ and the model classes it calls (FriendList.Status, DuelResult,
// AbstractPlayerInfoPacket.CHARNAME_MAX_LENGTH). Nothing here includes or mirrors a C++ serverpackets header. Every decode function consumes
// the body exactly and throws DecodeError otherwise (SM_VIEW_PLAYER_DETAILS' items excepted: its decoder reads the head and names the rest),
// and Java's literal constants are verified, not skipped.

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader and DecodeError

namespace aion::gameserver::scenario::decoders {

/** FriendList.Status ids (FriendList.java:147-159) */
constexpr uint8_t FRIEND_STATUS_OFFLINE = 0;
constexpr uint8_t FRIEND_STATUS_ONLINE = 1;
constexpr uint8_t FRIEND_STATUS_AWAY = 3;

/** SM_FRIEND_RESPONSE codes (SM_FRIEND_RESPONSE.java: TARGET_ADDED 0x0 .. REQUEST_ALREADY_RECEIVED 0x13) */
constexpr uint8_t FRIEND_RESPONSE_TARGET_ADDED = 0x00;
constexpr uint8_t FRIEND_RESPONSE_TARGET_OFFLINE = 0x01;
constexpr uint8_t FRIEND_RESPONSE_TARGET_NOT_FOUND = 0x03;
constexpr uint8_t FRIEND_RESPONSE_TARGET_REMOVED = 0x06;

/** SM_FRIEND_NOTIFY codes (SM_FRIEND_NOTIFY.java:16-24) */
constexpr uint8_t FRIEND_NOTIFY_LOGIN = 0;
constexpr uint8_t FRIEND_NOTIFY_LOGOUT = 1;
constexpr uint8_t FRIEND_NOTIFY_DELETED = 2;

/** SM_BLOCK_RESPONSE codes (SM_BLOCK_RESPONSE.java: BLOCK_SUCCESSFUL 0 .. EDIT_NOTE 5) */
constexpr uint8_t BLOCK_RESPONSE_BLOCK_SUCCESSFUL = 0;
constexpr uint8_t BLOCK_RESPONSE_TARGET_NOT_FOUND = 2;
constexpr uint8_t BLOCK_RESPONSE_CANT_BLOCK_SELF = 4;

/** SM_DUEL types (SM_DUEL.java: SM_DUEL_STARTED 0x00, SM_DUEL_RESULT 0x01) and DuelResult (DuelResult.java:7-9: message id, result id) */
constexpr uint8_t DUEL_TYPE_STARTED = 0x00;
constexpr uint8_t DUEL_TYPE_RESULT = 0x01;
constexpr uint8_t DUEL_RESULT_WON = 2;
constexpr uint8_t DUEL_RESULT_LOST = 0;
constexpr uint8_t DUEL_RESULT_DRAW = 1;
constexpr int32_t DUEL_WON_MESSAGE = 1300098;
constexpr int32_t DUEL_LOST_MESSAGE = 1300099;
constexpr int32_t DUEL_DRAW_MESSAGE = 1300100;

/** SM_TITLE_INFO actions (SM_TITLE_INFO.java: 0 list, 1 self set, 3 broad set, 4 mentor flag self, 5 broad mentor flag, 6 bonus title) */
constexpr uint8_t TITLE_ACTION_LIST = 0;
constexpr uint8_t TITLE_ACTION_SELF_SET = 1;
constexpr uint8_t TITLE_ACTION_BROAD_SET = 3;

/** AbstractPlayerInfoPacket.CHARNAME_MAX_LENGTH (AbstractPlayerInfoPacket.java:29) */
constexpr size_t CHARNAME_MAX_LENGTH = 25;

/** SM_FRIEND_RESPONSE.java writeImpl: writeS(playerName), writeC(code) */
struct FriendResponse {
	std::string name;
	uint8_t code = 0;
};
FriendResponse decodeFriendResponse(std::span<const uint8_t> body);

/** one friend of SM_FRIEND_LIST.java's loop */
struct FriendEntry {
	int32_t objectId = 0;
	std::string name;
	int32_t level = 0;
	int32_t classId = 0;
	uint8_t gender = 0;
	int32_t mapId = 0;
	int32_t lastOnline = 0;
	std::string note;
	uint8_t status = 0;
	int32_t houseAddress = 0;
	uint8_t doorState = 0;
	std::string memo;
};

/** SM_FRIEND_LIST.java writeImpl: writeH(-size), writeC(0), the friends */
struct FriendList {
	std::vector<FriendEntry> friends;
	const FriendEntry* find(int32_t objectId) const;
};
FriendList decodeFriendList(std::span<const uint8_t> body);

/** SM_FRIEND_UPDATE.java writeImpl (the friend found arm; the not-found arm writes nothing) */
struct FriendUpdate {
	bool empty = false;
	std::string name;
	int32_t level = 0;
	int32_t classId = 0;
	uint8_t gender = 0;
	int32_t mapId = 0;
	int32_t lastOnline = 0;
	std::string note;
	uint8_t status = 0;
};
FriendUpdate decodeFriendUpdate(std::span<const uint8_t> body);

/** SM_FRIEND_NOTIFY.java writeImpl: writeS(name), writeC(code) */
struct FriendNotify {
	std::string name;
	uint8_t code = 0;
};
FriendNotify decodeFriendNotify(std::span<const uint8_t> body);

/** SM_BLOCK_RESPONSE.java writeImpl: writeS(playerName), writeC(code) */
struct BlockResponse {
	std::string name;
	uint8_t code = 0;
};
BlockResponse decodeBlockResponse(std::span<const uint8_t> body);

/** SM_BLOCK_LIST.java writeImpl: writeH(-size), writeC(0), (writeS name, writeS reason) per blocked player */
struct BlockList {
	std::vector<std::pair<std::string, std::string>> blocked;
};
BlockList decodeBlockList(std::span<const uint8_t> body);

/** SM_UPDATE_NOTE.java writeImpl: writeD(targetObjId), writeS(note) */
struct UpdateNote {
	int32_t objectId = 0;
	std::string note;
};
UpdateNote decodeUpdateNote(std::span<const uint8_t> body);

/** SM_TITLE_INFO.java writeImpl, every action */
struct TitleInfo {
	uint8_t action = 0;
	/** action 1, 3, 4, 5: the title id (or the flag); action 6: the bonus title id */
	int32_t titleId = 0;
	/** action 3 and 5 */
	int32_t playerObjectId = 0;
	/** action 0: (title id, seconds until expiration) */
	std::vector<std::pair<int32_t, int32_t>> titles;
};
TitleInfo decodeTitleInfo(std::span<const uint8_t> body);

/** one row of SM_PLAYER_SEARCH.java's loop */
struct PlayerSearchEntry {
	int32_t worldId = 0;
	float x = 0, y = 0, z = 0;
	uint8_t classId = 0;
	uint8_t gender = 0;
	uint8_t level = 0;
	uint8_t groupState = 0;
	/** ChatUtil.toFactionPrefixedName(reader, player), written with writeS(text, CHARNAME_MAX_LENGTH + 2) */
	std::string name;
};
std::vector<PlayerSearchEntry> decodePlayerSearch(std::span<const uint8_t> body);

/** SM_VIEW_PLAYER_DETAILS.java writeImpl's head: writeD(targetObjId), writeC(11), writeH(itemSize); the items (writeItemInfo) are not read */
struct ViewPlayerDetailsHead {
	int32_t targetObjectId = 0;
	uint16_t itemCount = 0;
};
ViewPlayerDetailsHead decodeViewPlayerDetailsHead(std::span<const uint8_t> body);

/** SM_DUEL.java writeImpl: writeC(type); type 0x00 writeD(requesterObjId); 0x01 writeC(resultId), writeD(msgId), writeS(playerName); 0xE0 nothing */
struct Duel {
	uint8_t type = 0;
	int32_t requesterObjectId = 0;
	uint8_t resultId = 0;
	int32_t messageId = 0;
	std::string name;
};
Duel decodeDuel(std::span<const uint8_t> body);

/** SM_ABYSS_RANK.java writeImpl */
struct AbyssRank {
	int64_t ap = 0;
	int32_t gp = 0;
	int32_t rankId = 0;
	int32_t rankingListPosition = 0;
	int32_t allKill = 0;
	int32_t maxRank = 0;
	int32_t dailyKill = 0;
	int64_t dailyAp = 0;
	int32_t dailyGp = 0;
	int32_t weeklyKill = 0;
	int64_t weeklyAp = 0;
	int32_t weeklyGp = 0;
	int32_t lastKill = 0;
	int64_t lastAp = 0;
	int32_t lastGp = 0;
};
AbyssRank decodeAbyssRank(std::span<const uint8_t> body);

/** one row of SM_ABYSS_RANKING_PLAYERS.java's loop */
struct RankingListPlayerRow {
	int32_t position = 0;
	int32_t abyssRank = 0;
	int32_t oldPosition = 0;
	int32_t objectId = 0;
	int32_t classId = 0;
	uint8_t gender = 0;
	int64_t ap = 0;
	int32_t gp = 0;
	uint16_t level = 0;
	std::string name;
	std::string legionName;
};

/** SM_ABYSS_RANKING_PLAYERS.java writeImpl: writeD(race), writeD(lastUpdate), writeD(page), writeD(isEndPacket ? 0x7F : 0), the rows */
struct AbyssRankingPlayers {
	int32_t race = 0;
	int32_t lastUpdate = 0;
	int32_t page = 0;
	int32_t endFlag = 0;
	std::vector<RankingListPlayerRow> players;
};
AbyssRankingPlayers decodeAbyssRankingPlayers(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
