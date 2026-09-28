#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"

#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANK::SM_ABYSS_RANK(model::gameobjects::player::Player& player) : SM_ABYSS_RANK(player, std::nullopt) {
}

SM_ABYSS_RANK::SM_ABYSS_RANK(model::gameobjects::player::Player& player, std::optional<int32_t> rankingListPositionValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANK>), rank(player.getAbyssRank()),
	  rankingListPosition(!rankingListPositionValue ? services::abyss::AbyssRankingCache::getInstance().getRankingListPosition(player)
													 : *rankingListPositionValue) {
}

SM_ABYSS_RANK::~SM_ABYSS_RANK() = default;

void SM_ABYSS_RANK::writeImpl(AionConnection* con) {
	writeQ(rank->getAp());
	writeD(rank->getCurrentGP());
	writeD(model::gameobjects::detail::abyssRankId(rank->getRank()));
	writeD(rankingListPosition);
	writeD(0); // exp % removed with 4.5?
	writeD(rank->getAllKill());
	writeD(rank->getMaxRank());
	writeD(rank->getDailyKill());
	writeQ(rank->getDailyAP());
	writeD(rank->getDailyGP());
	writeD(rank->getWeeklyKill());
	writeQ(rank->getWeeklyAP());
	writeD(rank->getWeeklyGP());
	writeD(rank->getLastKill());
	writeQ(rank->getLastAP());
	writeD(rank->getLastGP());
	writeC(0x00); // unk
}

} // namespace aion::gameserver::network::aion::serverpackets
