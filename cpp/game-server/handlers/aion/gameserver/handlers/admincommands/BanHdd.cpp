#include "aion/gameserver/handlers/admincommands/BanHdd.h"

#include <string>

#include "aion/commons/database/DateTimeValue.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/services/ban/HDDBanService.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(BanHdd);

// parity= private static String SYNTAX = "Syntax: //banhdd <hdd_serial> <time_in_minutes|0 - infinite>"; (BanHdd.h)

BanHdd::BanHdd() : AdminCommand("banhdd") {
}

// Java BanHdd.java:24-37. Java catches every Exception: a missing parameter (ArrayIndexOutOfBounds) or a bad number answers the syntax.
// `timeMins * 60 * 1000` is Java int arithmetic: the pseudo infinity of 10 years wraps (proposed correction J-CP5-1)
void BanHdd::execute(Player& player, std::span<const std::string> params) {
	try {
		if (params.size() < 2) // parity: Java's ArrayIndexOutOfBoundsException of params[0] / params[1], explicit
			throw commons::utils::IndexOutOfBoundsException("params"); // parity: (the same)
		std::string hddSerial = params[0];
		int32_t timeMins = commons::utils::parseInt(params[1]);
		if (timeMins == 0)
			timeMins = 10 * 365 * 24 * 60;
		const int32_t millis = static_cast<int32_t>(static_cast<uint32_t>(timeMins) * 60u * 1000u); // parity: Java's int timeMins * 60 * 1000 (wraps)
		commons::database::Timestamp banTime{std::chrono::milliseconds(commons::utils::currentTimeMillis() + millis)}; // parity= Timestamp banTime = new Timestamp(System.currentTimeMillis() + timeMins * 60 * 1000);
		services::ban::HDDBanService::getInstance().addBan(hddSerial, banTime);
	} catch (const std::exception&) { // parity= } catch (Exception e) {
		PacketSendUtility::sendMessage(player, SYNTAX);
	}
}

} // namespace aion::gameserver::handlers::admincommands
