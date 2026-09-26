#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/services/abyss/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::services::abyss {

/**
 * C++: a static-only class (hub-headers.md §11.1). AbyssRankDAO.h is included for the nested record AbyssRankDAO::RankingListPlayerGp
 * (hub-headers.md §9.3).
 *
 * @author ATracer, Neon
 */
class AbyssRankUpdateService {
private:
	AbyssRankUpdateService() = delete;
public:
	static void scheduleUpdate();
	/** Perform update of all ranks */
	static void performUpdate();
private:
	/**
	 * Update player ranks based on quota for all players (online/offline)
	 *  @param race
	 *          the race that will be updated
	 */
	static void updateQuotaRanksForRace(model::Race race, utils::stats::AbyssRankEnum minRank);
	static int32_t selectAndUpdateQuotaRank(utils::stats::AbyssRankEnum rank, const std::vector<dao::AbyssRankDAO::RankingListPlayerGp>& rankingList, int32_t usedQuota);
	static void updateToNoQuotaRank(const std::unordered_map<int32_t, int32_t>& apByPlayerId);
	static void updateRankTo(utils::stats::AbyssRankEnum newRank, int32_t playerId, int32_t rankingPosition);
	static void updateDailyGpLoss();
};

} // namespace aion::gameserver::services::abyss
