#include "aion/gameserver/handlers/admincommands/Ranking.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/services/abyss/AbyssRankUpdateService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Ranking);

Ranking::Ranking() : AdminCommand("ranking", "Abyss rank control.", "update - Runs the daily Abyss rank update task.\n") {
}

// Java Ranking.java:22-28
void Ranking::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() == 1 && commons::utils::StringUtils::equalsIgnoreCase("update", params[0]))
		services::abyss::AbyssRankUpdateService::performUpdate();
	else
		sendInfo(admin);
}

} // namespace aion::gameserver::handlers::admincommands
