#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::abyss {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.abyss.AbyssRankUpdateService");

// callback at AbyssRankUpdateService.java:30 (fieldmap key AbyssRankUpdateService@L30:38)
// callback at AbyssRankUpdateService.java:33 (fieldmap key AbyssRankUpdateService@L33:38)
void AbyssRankUpdateService::scheduleUpdate() {
	AION_UNPORTED();
}

// callback at AbyssRankUpdateService.java:40 (fieldmap key AbyssRankUpdateService@L40:44)
void AbyssRankUpdateService::performUpdate() {
	AION_UNPORTED();
}

void AbyssRankUpdateService::updateQuotaRanksForRace(model::Race race, utils::stats::AbyssRankEnum minRank) {
	AION_UNPORTED();
}

int32_t AbyssRankUpdateService::selectAndUpdateQuotaRank(utils::stats::AbyssRankEnum rank, const std::vector<dao::AbyssRankDAO::RankingListPlayerGp>& rankingList, int32_t usedQuota) {
	AION_UNPORTED();
}

void AbyssRankUpdateService::updateToNoQuotaRank(const std::unordered_map<int32_t, int32_t>& apByPlayerId) {
	AION_UNPORTED();
}

void AbyssRankUpdateService::updateRankTo(utils::stats::AbyssRankEnum newRank, int32_t playerId, int32_t rankingPosition) {
	AION_UNPORTED();
}

void AbyssRankUpdateService::updateDailyGpLoss() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::abyss
