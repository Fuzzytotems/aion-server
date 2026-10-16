#include "aion/gameserver/handlers/admincommands/Time.h"

#include <format>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GAME_TIME.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Time);

Time::Time()
	: AdminCommand("time", "Changes the game time.",
		  "<dawn|day|dusk|night> - Sets the specified day time.\n"
		  "<0-23> - Sets the specified hour.\n"
		  "<0-23> <0-59> - Sets the specified hour and minute.\n") {
}

// Java Time.java:24-71
void Time::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	int32_t hour;
	int32_t minute = 0;

	if (equalsIgnoreCase(params[0], "night")) {
		hour = 22;
	} else if (equalsIgnoreCase(params[0], "dusk")) {
		hour = 18;
	} else if (equalsIgnoreCase(params[0], "day")) {
		hour = 9;
	} else if (equalsIgnoreCase(params[0], "dawn")) {
		hour = 4;
	} else {
		hour = commons::utils::parseInt(params[0]);
		if (hour < 0 || hour > 23) {
			sendInfo(admin, "Hour must be between 0 and 23.");
			return;
		}
		if (params.size() == 2) {
			minute = commons::utils::parseInt(params[1]);
			if (minute < 0 || minute > 59) {
				sendInfo(admin, "Minute must be between 0 and 59.");
				return;
			}
		}
	}

	runtime::Ptr<GameTime> gameTime = GameTimeService::getInstance().getGameTime();
	int32_t hourOffset = hour - gameTime->getHour(); // hour offset inside the same day
	int32_t minutesToAdd = 60 * hourOffset;
	if (minute == 0) {
		minutesToAdd -= gameTime->getMinute();
	} else {
		int32_t minuteOffset = minute - gameTime->getMinute();
		minutesToAdd += minuteOffset;
	}
	gameTime->addMinutes(minutesToAdd);
	PacketSendUtility::broadcastToWorld(SM_GAME_TIME());
	sendInfo(admin, "You changed the time to " + std::to_string(gameTime->getHour()) + ":" + std::format("{:02d}", gameTime->getMinute()) + "."); // parity= sendInfo(admin, "You changed the time to " + gameTime.getHour() + ":" + String.format("%02d", gameTime.getMinute()) + ".");
}

} // namespace aion::gameserver::handlers::admincommands
