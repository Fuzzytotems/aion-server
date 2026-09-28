#include "aion/gameserver/configs/main/RankingConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::configs::main {

void RankingConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.topranking.updaterule", TOP_RANKING_UPDATE_RULE, "0 0 0 ? * *");
	AION_BIND(p, "gameserver.topranking.daily.gploss.time", TOP_RANKING_DAILY_GP_LOSS_TIME, "0 0 12 ? * *");
	AION_BIND(p, "gameserver.topranking.legion_limit", RANKING_LIST_LEGION_LIMIT, "50");
	AION_BIND(p, "gameserver.topranking.max.offline.days", TOP_RANKING_MAX_OFFLINE_DAYS, "0");
	AION_BIND(p, "gameserver.topranking.xform.min_rank", XFORM_MIN_RANK, "STAR5_OFFICER");
	AION_BIND_PATTERN(p, "^gameserver\\.topranking\\.quota\\.(.+)", TOP_RANKING_QUOTA);
	AION_BIND_PATTERN(p, "^gameserver\\.topranking\\.gp_loss\\.(.+)", TOP_RANKING_GP_LOSS);
}

} // namespace aion::gameserver::configs::main
