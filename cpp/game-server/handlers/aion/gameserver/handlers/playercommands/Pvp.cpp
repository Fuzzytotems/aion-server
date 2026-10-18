#include "aion/gameserver/handlers/playercommands/Pvp.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Pvp);

// Java Pvp.java:12-18
Pvp::Pvp()
	: PlayerCommand("pvp", "Join the custom PvP-Map where you can fight against the opposing faction.",
		  "join - Joins the PvP-Map.\n"
		  "leave - Leaves the PvP-Map.\n"
		  "info - Shows how many players are on the PvP-Map.\n") {
}

// Java Pvp.java:20-34
void Pvp::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params.length == 0) {
		sendInfo(player);
	} else if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "join")) { // parity= } else if (params[0].equalsIgnoreCase("join")) {
		custom::pvpmap::PvpMapService::getInstance().joinMap(player);
	} else if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "leave")) { // parity= } else if (params[0].equalsIgnoreCase("leave")) {
		custom::pvpmap::PvpMapService::getInstance().leaveMap(player);
	} else if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "info")) { // parity= } else if (params[0].equalsIgnoreCase("info")) {
		int32_t size = custom::pvpmap::PvpMapService::getInstance().getParticipantsSize();
		sendInfo(player, "There " + std::string(size == 1 ? "is" : "are") + " currently " + (size == 0 ? std::string("no") : std::to_string(size)) + " player" + (size != 1 ? "s" : "") + " on the map."); // parity= sendInfo(player, "There " + (size == 1 ? "is" : "are") + " currently " + (size == 0 ? "no" : size) + " player" + (size != 1 ? "s" : "") +" on the map.");
	} else {
		sendInfo(player);
	}
}

} // namespace aion::gameserver::handlers::playercommands
