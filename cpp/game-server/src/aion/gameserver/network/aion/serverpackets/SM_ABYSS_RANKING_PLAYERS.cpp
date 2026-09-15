#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_PLAYERS.h"

#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANKING_PLAYERS::SM_ABYSS_RANKING_PLAYERS(int32_t lastUpdateValue, model::Race raceValue)
	: SM_ABYSS_RANKING_PLAYERS(lastUpdateValue, {}, raceValue, 0, false) {
}

SM_ABYSS_RANKING_PLAYERS::SM_ABYSS_RANKING_PLAYERS(int32_t lastUpdateValue,
	const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListPlayer>>& playersValue, model::Race raceValue, int32_t pageValue,
	bool isEndPacketValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANKING_PLAYERS>), players(playersValue.begin(), playersValue.end()), lastUpdate(lastUpdateValue),
	  race(model::getRaceId(raceValue)), page(pageValue), isEndPacket(isEndPacketValue) {
}

SM_ABYSS_RANKING_PLAYERS::~SM_ABYSS_RANKING_PLAYERS() = default;

void SM_ABYSS_RANKING_PLAYERS::writeImpl(AionConnection* con) {
	writeD(race);
	writeD(lastUpdate);
	writeD(page);
	writeD(isEndPacket ? 0x7F : 0); // 0:Nothing 1:Update Table
	writeH(static_cast<int32_t>(players.size()));
	for (const runtime::Ref<dao::AbyssRankDAO::RankingListPlayer>& player : players) {
		writeD(player->position());
		writeD(player->abyssRank());
		writeD(player->oldPosition());
		writeD(player->id());
		writeD(race);
		writeD(model::getClassId(player->playerClass()));
		writeC(model::getGenderId(player->gender()));
		writeC(0); // unk
		writeC(0); // unk
		writeC(0); // unk
		writeQ(player->ap());
		writeD(player->gp());
		writeH(player->level());
		// Two strings actually: player name + server name suffix (eg., SL for FastTrack)
		writeS(player->name(), AbstractPlayerInfoPacket::CHARNAME_MAX_LENGTH);
		writeS(player->legionName(), 42);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
