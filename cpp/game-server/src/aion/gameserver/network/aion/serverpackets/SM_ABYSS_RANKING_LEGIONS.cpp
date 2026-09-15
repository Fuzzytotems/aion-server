#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_LEGIONS.h"

#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANKING_LEGIONS::SM_ABYSS_RANKING_LEGIONS(int32_t updateTimeValue, model::Race raceValue)
	: SM_ABYSS_RANKING_LEGIONS(updateTimeValue, {}, raceValue, false) {
}

SM_ABYSS_RANKING_LEGIONS::SM_ABYSS_RANKING_LEGIONS(int32_t updateTimeValue,
	const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListLegion>>& rankingListValue, model::Race raceValue)
	: SM_ABYSS_RANKING_LEGIONS(updateTimeValue, rankingListValue, raceValue, true) {
}

SM_ABYSS_RANKING_LEGIONS::SM_ABYSS_RANKING_LEGIONS(int32_t updateTimeValue,
	const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListLegion>>& rankingListValue, model::Race raceValue, bool clearListBeforeUpdateValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANKING_LEGIONS>), rankingList(rankingListValue.begin(), rankingListValue.end()), race(raceValue),
	  updateTime(updateTimeValue), clearListBeforeUpdate(clearListBeforeUpdateValue) {
}

SM_ABYSS_RANKING_LEGIONS::~SM_ABYSS_RANKING_LEGIONS() = default;

void SM_ABYSS_RANKING_LEGIONS::writeImpl(AionConnection* con) {
	writeD(model::getRaceId(race));
	writeD(updateTime);
	writeD(clearListBeforeUpdate ? 1 : 0);
	writeD(clearListBeforeUpdate ? 1 : 0);
	writeH(static_cast<int32_t>(rankingList.size()));
	for (const runtime::Ref<dao::AbyssRankDAO::RankingListLegion>& legion : rankingList) {
		writeD(legion->position());
		writeD(legion->oldPosition());
		writeD(legion->id());
		writeD(model::getRaceId(race));
		writeC(legion->level());
		writeD(legion->memberCount());
		writeQ(legion->contributionPoints());
		writeS(legion->name(), 40);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
