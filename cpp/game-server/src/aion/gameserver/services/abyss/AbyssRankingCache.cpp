#include "aion/gameserver/services/abyss/AbyssRankingCache.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::abyss {

AbyssRankingCache::AbyssRankingCache() {
	AION_UNPORTED();
}

AbyssRankingCache::~AbyssRankingCache() = default;

AbyssRankingCache& AbyssRankingCache::getInstance() {
	static AbyssRankingCache instance; // Java SingletonHolder
	return instance;
}

void AbyssRankingCache::refreshCache() {
	AION_UNPORTED();
}

void AbyssRankingCache::reloadRankings() {
	AION_UNPORTED();
}

std::vector<std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_PLAYERS>> AbyssRankingCache::getPlayerRankListPackets(int32_t updateTime, model::Race race, const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListPlayer>>& list) {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcArrayList<std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_PLAYERS>>> AbyssRankingCache::getPlayers(model::Race race) {
	AION_UNPORTED();
}

std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_LEGIONS> AbyssRankingCache::getLegions(model::Race race) {
	AION_UNPORTED();
}

int32_t AbyssRankingCache::getRankingListPosition(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t AbyssRankingCache::getRankingListPosition(model::team::legion::Legion& legion) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::abyss
