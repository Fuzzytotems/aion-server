#include "aion/gameserver/handlers/admincommands/Ahserion.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Ahserion);

// Java Ahserion.java:12-17
Ahserion::Ahserion()
	: AdminCommand("ahserion", "Starts/stops Ahserion's Flight.",
		  "start - Starts Ahserion's Flight.\n"
		  "stop - Stops Ahserion's Flight.\n") {
}

// Java Ahserion.java:19-41
void Ahserion::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params.length == 0) {
		sendInfo(admin);
		return;
	}

	if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "start")) { // parity= if (params[0].equalsIgnoreCase("start")) {
		if (services::panesterra::PanesterraService::getInstance().isAhserionRaidStarted()) {
			sendInfo(admin, "Ahserion's Flight is already running.");
		} else {
			services::panesterra::PanesterraService::getInstance().startAhserionRaid();
			sendInfo(admin, "Started Ahserion's Flight.");
		}
	} else if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "stop")) { // parity= } else if (params[0].equalsIgnoreCase("stop")) {
		if (!services::panesterra::PanesterraService::getInstance().isAhserionRaidStarted()) {
			sendInfo(admin, "Ahserion's Flight is not running.");
		} else {
			services::panesterra::PanesterraService::getInstance().stopAhserionRaid();
			sendInfo(admin, "Stopped Ahserion's Flight.");
		}
	}
}

} // namespace aion::gameserver::handlers::admincommands
