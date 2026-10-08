#include "aion/gameserver/handlers/playercommands/Advent.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/services/reward/AdventService.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Advent);

Advent::Advent() // parity= Advent(); PlayerCommand("advent", "Gets your advent reward for today.", "show - Shows today's reward.\nget - Gets your reward for today on this character.\n%s Only one character per account can receive this reward!\n".formatted(ChatUtil.color("ATTENTION:", Color.PINK)));
	: PlayerCommand("advent", "Gets your advent reward for today.", // parity: (the same)
		  "show - Shows today's reward.\n" // parity: (the same)
		  "get - Gets your reward for today on this character.\n" + // parity: (the same)
			  ChatUtil::color("ATTENTION:", utils::JavaColor::PINK) + " Only one character per account can receive this reward!\n") { // parity: (the same)
}

// Java Advent.java:29-37
void Advent::execute(Player& player, std::span<const std::string> params) {
	if (params.size() != 1)
		sendInfo(player);
	else if (commons::utils::StringUtils::equalsIgnoreCase("show", params[0]))
		services::reward::AdventService::getInstance().showTodaysReward(player);
	else if (commons::utils::StringUtils::equalsIgnoreCase("get", params[0]))
		services::reward::AdventService::getInstance().redeemReward(player);
}

// Java Advent.java:39-55
bool Advent::validateAccess(Player& player) {
	if (!PlayerCommand::validateAccess(player))
		return false;
	if (!configs::main::EventsConfig::ENABLE_ADVENT_CALENDAR.load()) { // parity= if (!EventsConfig.ENABLE_ADVENT_CALENDAR) {
		if (player.isStaff())
			sendInfo(player, "The advent calendar is currently disabled.");
		return false;
	}
	if (!services::reward::AdventService::getInstance().isAdventSeason()) {
		if (player.isStaff())
			sendInfo(player, "This command is only active during the Advent season.");
		return false;
	}
	return true;
}

} // namespace aion::gameserver::handlers::playercommands
