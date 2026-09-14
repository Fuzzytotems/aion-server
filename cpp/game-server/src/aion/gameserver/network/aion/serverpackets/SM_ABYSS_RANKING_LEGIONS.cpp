#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_LEGIONS.h"

#include "aion/gameserver/runtime/base/Unported.h"
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
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
