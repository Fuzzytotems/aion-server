#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"

#include <string>

#include <optional>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::services::abyss {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.abyss.AbyssRankUpdateService");

using utils::stats::AbyssRankEnum;

// callback at AbyssRankUpdateService.java:30 (fieldmap key AbyssRankUpdateService@L30:38)
// callback at AbyssRankUpdateService.java:33 (fieldmap key AbyssRankUpdateService@L33:38)
void AbyssRankUpdateService::scheduleUpdate() {
	const auto expressionText = [](const cron::CronExpression* expression) { return expression != nullptr ? expression->toString() : std::string("null"); };
	const auto require = [](const cron::CronExpression* expression) -> const cron::CronExpression& {
		if (expression == nullptr)
			throw runtime::NullPointerException("cronExpression"); // Java: CronService.schedule with a null expression
		return *expression;
	};
	const cron::CronExpression* updateRule = configs::main::RankingConfig::TOP_RANKING_UPDATE_RULE.load();
	log.info("Scheduling ranking update task based on cron expression: " + expressionText(updateRule));
	cron::CronService::getInstance().schedule([] { performUpdate(); }, require(updateRule), true);

	const cron::CronExpression* gpLossTime = configs::main::RankingConfig::TOP_RANKING_DAILY_GP_LOSS_TIME.load();
	log.info("Scheduling daily GP loss task based on cron expression: " + expressionText(gpLossTime));
	cron::CronService::getInstance().schedule([] { updateDailyGpLoss(); }, require(gpLossTime), true);
}

// Java AbyssRankUpdateService.java:39-69; the lambda at :40 (fieldmap key AbyssRankUpdateService@L40:44) captures nothing
void AbyssRankUpdateService::performUpdate() {
	utils::ThreadPoolManager::getInstance().schedule([] {
		log.info("AbyssRankUpdateService: Executing rank update...");
		int64_t startTime = commons::utils::currentTimeMillis();

		// update and store rank statistics for all online players (offline players update on login)
		world::World::getInstance().forEachPlayer([](model::gameobjects::player::Player& player) {
			player.getAbyssRank()->doUpdate();
			dao::AbyssRankDAO::storeAbyssRank(player);
		});

		// Java: the ranks with a required GP, the one with the smallest id (orElse STAR1_OFFICER) and the largest quota (orElse 1000)
		std::optional<AbyssRankEnum> minGpRankFound;
		std::optional<int32_t> playerLimitFound;
		for (size_t i = 0; i < xml::EnumTraits<AbyssRankEnum>::names.size(); i++) {
			const AbyssRankEnum rank = static_cast<AbyssRankEnum>(i);
			if (utils::stats::requiredGP(rank) <= 0)
				continue;
			if (!minGpRankFound || utils::stats::id(rank) < utils::stats::id(*minGpRankFound))
				minGpRankFound = rank;
			const int32_t quota = utils::stats::quota(rank);
			if (!playerLimitFound || quota > *playerLimitFound)
				playerLimitFound = quota;
		}
		AbyssRankEnum minGpRank = minGpRankFound.value_or(AbyssRankEnum::STAR1_OFFICER);
		int32_t playerLimit = playerLimitFound.value_or(1000);

		// update and store player & legion DB rank_pos entries (for ▼/▲/= trend in the ranking table)
		dao::AbyssRankDAO::updateRankingLists(configs::main::RankingConfig::TOP_RANKING_MAX_OFFLINE_DAYS.load(), playerLimit,
			configs::main::RankingConfig::RANKING_LIST_LEGION_LIMIT.load());
		// update and store GP ranks
		updateQuotaRanksForRace(model::Race::ASMODIANS, minGpRank);
		updateQuotaRanksForRace(model::Race::ELYOS, minGpRank);

		// update ranking cache
		AbyssRankingCache::getInstance().reloadRankings();

		log.info("AbyssRankUpdateService: Finished in " + std::to_string((commons::utils::currentTimeMillis() - startTime) / 1000) + "s");
	}, 1000);
}

// Java AbyssRankUpdateService.java:78-92
void AbyssRankUpdateService::updateQuotaRanksForRace(model::Race race, utils::stats::AbyssRankEnum minRank) {
	std::optional<std::vector<dao::AbyssRankDAO::RankingListPlayerGp>> rankingListSorted = dao::AbyssRankDAO::loadRankingListPlayersGp(race);
	if (!rankingListSorted)
		return;

	// calculate and set new GP ranks
	int32_t usedQuota = 0;
	for (int32_t i = utils::stats::id(AbyssRankEnum::SUPREME_COMMANDER); i >= utils::stats::id(minRank); i--)
		usedQuota = selectAndUpdateQuotaRank(utils::stats::getRankById(i), *rankingListSorted, usedQuota);

	// set all players with an old GP rank to AP ranks if they hold no rank position anymore
	std::optional<std::unordered_map<int32_t, int32_t>> apByPlayerId = dao::AbyssRankDAO::loadApOfPlayersNotInRankingList(race, minRank);
	if (apByPlayerId)
		updateToNoQuotaRank(*apByPlayerId);
}

// Java AbyssRankUpdateService.java:94-105. Java removes each ranked player through the iterator; they are always a prefix of the list (the
// loop breaks at the first player who is not ranked), so the C++ erases that prefix once the loop ends
int32_t AbyssRankUpdateService::selectAndUpdateQuotaRank(utils::stats::AbyssRankEnum rank, std::vector<dao::AbyssRankDAO::RankingListPlayerGp>& rankingList,
	int32_t usedQuota) {
	size_t removed = 0;
	for (; removed < rankingList.size(); removed++) {
		const dao::AbyssRankDAO::RankingListPlayerGp& rankingListEntry = rankingList[removed];
		if (usedQuota >= utils::stats::quota(rank) || rankingListEntry.gp() < utils::stats::requiredGP(rank))
			break;
		// remove player and update its rank
		updateRankTo(rank, rankingListEntry.playerId(), rankingListEntry.position());
		usedQuota++;
	}
	rankingList.erase(rankingList.begin(), rankingList.begin() + static_cast<std::ptrdiff_t>(removed));
	return usedQuota;
}

// Java AbyssRankUpdateService.java:107-112 (HashMap.forEach: no order the rank update depends on)
void AbyssRankUpdateService::updateToNoQuotaRank(const std::unordered_map<int32_t, int32_t>& apByPlayerId) {
	for (const auto& [playerId, ap] : apByPlayerId) {
		AbyssRankEnum rank = utils::stats::getRankForPoints(ap, 0); // no GP -> no officer ranks
		updateRankTo(rank, playerId, 0);
	}
}

// Java AbyssRankUpdateService.java:114-128
void AbyssRankUpdateService::updateRankTo(utils::stats::AbyssRankEnum newRank, int32_t playerId, int32_t rankingPosition) {
	// check if rank has changed for online players
	runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(playerId);
	if (player != nullptr) {
		bool rankChanged = player->getAbyssRank()->getRank() != newRank;
		if (rankChanged) {
			player->getAbyssRank()->setRank(newRank);
			// save to db now, so cache reload loads the correct rank for online players
			dao::AbyssRankDAO::updateAbyssRank(playerId, newRank);
		}
		AbyssPointsService::onRankChanged(*player, false, rankChanged, rankingPosition);
	} else {
		dao::AbyssRankDAO::updateAbyssRank(playerId, newRank);
	}
}

// Java AbyssRankUpdateService.java:130-140
void AbyssRankUpdateService::updateDailyGpLoss() {
	for (size_t i = 0; i < xml::EnumTraits<AbyssRankEnum>::names.size(); i++) {
		const AbyssRankEnum rank = static_cast<AbyssRankEnum>(i);
		if (utils::stats::gpLossPerDay(rank) > 0)
			dao::AbyssRankDAO::dailyUpdateGp(rank);
	}
	for (const runtime::Ptr<model::gameobjects::player::Player>& p : world::World::getInstance().getAllPlayers()) {
		if (utils::stats::gpLossPerDay(p->getAbyssRank()->getRank()) > 0) {
			GloryPointsService::addGp(p->getObjectId(), -utils::stats::gpLossPerDay(p->getAbyssRank()->getRank()));
		}
	}
}

} // namespace aion::gameserver::services::abyss
