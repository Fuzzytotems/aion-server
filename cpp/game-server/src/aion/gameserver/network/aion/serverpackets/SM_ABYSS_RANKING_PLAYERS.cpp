#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_PLAYERS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANKING_PLAYERS::SM_ABYSS_RANKING_PLAYERS(int32_t lastUpdateValue, model::Race raceValue)
	: SM_ABYSS_RANKING_PLAYERS(lastUpdateValue, {}, raceValue, 0, false) {
}

SM_ABYSS_RANKING_PLAYERS::SM_ABYSS_RANKING_PLAYERS(int32_t lastUpdateValue,
	const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListPlayer>>& playersValue, model::Race raceValue, int32_t pageValue,
	bool isEndPacketValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANKING_PLAYERS>) {
	AION_UNPORTED();
}

SM_ABYSS_RANKING_PLAYERS::~SM_ABYSS_RANKING_PLAYERS() = default;

void SM_ABYSS_RANKING_PLAYERS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
