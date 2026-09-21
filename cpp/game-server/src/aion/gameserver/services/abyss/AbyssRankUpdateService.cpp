#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"

#include <string>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::services::abyss {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.abyss.AbyssRankUpdateService");

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

// callback at AbyssRankUpdateService.java:40 (fieldmap key AbyssRankUpdateService@L40:44)
void AbyssRankUpdateService::performUpdate() {
	// M5a (plan E1-04): the schedule is ported, the rank update runs with the abyss rank work of M5b; a cron job must not throw meanwhile
	AION_PARTIAL("the abyss rank update is not ported yet (M5b)");
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
	// M5a (plan E1-04): the schedule is ported, the GP loss runs with the abyss rank work of M5b; a cron job must not throw meanwhile
	AION_PARTIAL("the daily GP loss is not ported yet (M5b)");
}

} // namespace aion::gameserver::services::abyss
