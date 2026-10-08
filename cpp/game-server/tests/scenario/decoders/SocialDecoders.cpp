#include "decoders/SocialDecoders.h"

#include <string>

namespace aion::gameserver::scenario::decoders {

namespace {

/** writeH(-size): the negated count as an unsigned short */
uint16_t negatedCount(BodyReader& reader) {
	return static_cast<uint16_t>(-static_cast<int16_t>(reader.H()));
}

} // namespace

// ---- friends ------------------------------------------------------------------------------------------------------------------------------

FriendResponse decodeFriendResponse(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FRIEND_RESPONSE");
	FriendResponse response;
	response.name = reader.S(); // SM_FRIEND_RESPONSE.java writeS(playerName)
	response.code = reader.C(); // writeC(code)
	reader.expectFullyConsumed();
	return response;
}

const FriendEntry* FriendList::find(int32_t objectId) const {
	for (const FriendEntry& entry : friends)
		if (entry.objectId == objectId)
			return &entry;
	return nullptr;
}

FriendList decodeFriendList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FRIEND_LIST");
	FriendList list;
	const uint16_t count = negatedCount(reader); // SM_FRIEND_LIST.java writeH(-list.getSize())
	reader.expectC(0, "SM_FRIEND_LIST writeC(0)"); // unk
	for (uint16_t i = 0; i < count; i++) {
		FriendEntry entry;
		entry.objectId = reader.D();
		entry.name = reader.S();
		entry.level = reader.D();
		entry.classId = reader.D();
		entry.gender = reader.C();
		entry.mapId = reader.D();
		entry.lastOnline = reader.D(); // 0 when ONLINE, else getLastOnlineEpochSeconds()
		entry.note = reader.S();
		entry.status = reader.C();
		entry.houseAddress = reader.D();
		entry.doorState = reader.C();
		entry.memo = reader.S();
		list.friends.push_back(std::move(entry));
	}
	reader.expectFullyConsumed();
	return list;
}

FriendUpdate decodeFriendUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FRIEND_UPDATE");
	FriendUpdate update;
	if (reader.remaining() == 0) { // SM_FRIEND_UPDATE.java: a friend not on the reader's list writes nothing (a log.debug)
		update.empty = true;
		return update;
	}
	update.name = reader.S();
	update.level = reader.D();
	update.classId = reader.D();
	update.gender = reader.C();
	update.mapId = reader.D();
	update.lastOnline = reader.D();
	update.note = reader.S();
	update.status = reader.C();
	reader.expectFullyConsumed();
	return update;
}

FriendNotify decodeFriendNotify(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_FRIEND_NOTIFY");
	FriendNotify notify;
	notify.name = reader.S();
	notify.code = reader.C();
	reader.expectFullyConsumed();
	return notify;
}

// ---- blocks -------------------------------------------------------------------------------------------------------------------------------

BlockResponse decodeBlockResponse(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_BLOCK_RESPONSE");
	BlockResponse response;
	response.name = reader.S();
	response.code = reader.C();
	reader.expectFullyConsumed();
	return response;
}

BlockList decodeBlockList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_BLOCK_LIST");
	BlockList list;
	const uint16_t count = negatedCount(reader); // SM_BLOCK_LIST.java writeH(-list.getSize())
	reader.expectC(0, "SM_BLOCK_LIST writeC(0)");
	for (uint16_t i = 0; i < count; i++) {
		std::string name = reader.S();
		std::string reason = reader.S();
		list.blocked.emplace_back(std::move(name), std::move(reason));
	}
	reader.expectFullyConsumed();
	return list;
}

// ---- note, titles -----------------------------------------------------------------------------------------------------------------

UpdateNote decodeUpdateNote(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_UPDATE_NOTE");
	UpdateNote note;
	note.objectId = reader.D();
	note.note = reader.S();
	reader.expectFullyConsumed();
	return note;
}

TitleInfo decodeTitleInfo(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_TITLE_INFO");
	TitleInfo info;
	info.action = reader.C();
	switch (info.action) {
		case 0: { // SM_TITLE_INFO.java case 0: writeC(0x00), writeH(size), (writeD id, writeD secondsUntilExpiration) per title
			reader.expectC(0x00, "SM_TITLE_INFO list writeC(0x00)");
			const uint16_t count = reader.H();
			for (uint16_t i = 0; i < count; i++) {
				const int32_t id = reader.D();
				info.titles.emplace_back(id, reader.D());
			}
			break;
		}
		case 1:
		case 4:
			info.titleId = reader.H();
			break;
		case 3:
		case 5:
			info.playerObjectId = reader.D();
			info.titleId = reader.H();
			break;
		case 6:
			info.titleId = reader.H();
			break;
		default: // the switch has no default: any other action writes the action byte alone
			break;
	}
	reader.expectFullyConsumed();
	return info;
}

// ---- lookups ------------------------------------------------------------------------------------------------------------------------------

std::vector<PlayerSearchEntry> decodePlayerSearch(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PLAYER_SEARCH");
	std::vector<PlayerSearchEntry> entries;
	const uint16_t count = reader.H(); // SM_PLAYER_SEARCH.java writeH(players.size())
	for (uint16_t i = 0; i < count; i++) {
		PlayerSearchEntry entry;
		entry.worldId = reader.D();
		entry.x = reader.F();
		entry.y = reader.F();
		entry.z = reader.F();
		entry.classId = reader.C();
		entry.gender = reader.C();
		entry.level = reader.C();
		entry.groupState = reader.C();
		entry.name = reader.S(CHARNAME_MAX_LENGTH + 2);
		entries.push_back(std::move(entry));
	}
	reader.expectFullyConsumed();
	return entries;
}

ViewPlayerDetailsHead decodeViewPlayerDetailsHead(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_VIEW_PLAYER_DETAILS");
	ViewPlayerDetailsHead head;
	head.targetObjectId = reader.D();
	reader.expectC(11, "SM_VIEW_PLAYER_DETAILS writeC(11)");
	head.itemCount = reader.H();
	if (head.itemCount == 0)
		reader.expectFullyConsumed();
	else if (reader.remaining() == 0)
		reader.fail("an item count without items");
	return head;
}

// ---- duel ---------------------------------------------------------------------------------------------------------------------------------

Duel decodeDuel(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DUEL");
	Duel duel;
	duel.type = reader.C();
	switch (duel.type) {
		case 0x00:
			duel.requesterObjectId = reader.D();
			break;
		case 0x01:
			duel.resultId = reader.C();
			duel.messageId = reader.D();
			duel.name = reader.S();
			break;
		case 0xE0:
			break;
		default: // SM_DUEL.java throws IllegalArgumentException for any other type: such a packet is never written
			reader.fail("SM_DUEL type " + std::to_string(duel.type) + " is never written");
	}
	reader.expectFullyConsumed();
	return duel;
}

// ---- abyss --------------------------------------------------------------------------------------------------------------------------------

AbyssRank decodeAbyssRank(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ABYSS_RANK");
	AbyssRank rank;
	rank.ap = reader.Q();
	rank.gp = reader.D();
	rank.rankId = reader.D();
	rank.rankingListPosition = reader.D();
	reader.expectD(0, "SM_ABYSS_RANK writeD(0) exp % removed with 4.5");
	rank.allKill = reader.D();
	rank.maxRank = reader.D();
	rank.dailyKill = reader.D();
	rank.dailyAp = reader.Q();
	rank.dailyGp = reader.D();
	rank.weeklyKill = reader.D();
	rank.weeklyAp = reader.Q();
	rank.weeklyGp = reader.D();
	rank.lastKill = reader.D();
	rank.lastAp = reader.Q();
	rank.lastGp = reader.D();
	reader.expectC(0x00, "SM_ABYSS_RANK writeC(0x00) unk");
	reader.expectFullyConsumed();
	return rank;
}

AbyssRankingPlayers decodeAbyssRankingPlayers(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_ABYSS_RANKING_PLAYERS");
	AbyssRankingPlayers ranking;
	ranking.race = reader.D();
	ranking.lastUpdate = reader.D();
	ranking.page = reader.D();
	ranking.endFlag = reader.D(); // isEndPacket ? 0x7F : 0
	if (ranking.endFlag != 0 && ranking.endFlag != 0x7F)
		reader.fail("the end flag is neither 0 nor 0x7F");
	const uint16_t count = reader.H();
	for (uint16_t i = 0; i < count; i++) {
		RankingListPlayerRow row;
		row.position = reader.D();
		row.abyssRank = reader.D();
		row.oldPosition = reader.D();
		row.objectId = reader.D();
		reader.expectD(ranking.race, "SM_ABYSS_RANKING_PLAYERS writeD(race) per row");
		row.classId = reader.D();
		row.gender = reader.C();
		reader.expectC(0, "SM_ABYSS_RANKING_PLAYERS writeC(0) unk");
		reader.expectC(0, "SM_ABYSS_RANKING_PLAYERS writeC(0) unk");
		reader.expectC(0, "SM_ABYSS_RANKING_PLAYERS writeC(0) unk");
		row.ap = reader.Q();
		row.gp = reader.D();
		row.level = reader.H();
		row.name = reader.S(CHARNAME_MAX_LENGTH);
		row.legionName = reader.S(42);
		ranking.players.push_back(std::move(row));
	}
	reader.expectFullyConsumed();
	return ranking;
}

} // namespace aion::gameserver::scenario::decoders
