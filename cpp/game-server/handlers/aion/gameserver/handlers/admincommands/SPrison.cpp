#include "aion/gameserver/handlers/admincommands/SPrison.h"

#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SPrison);

SPrison::SPrison() : AdminCommand("sprison") {
}

// Java SPrison.java:22-46. Java catches every Exception: two parameters (params[2] out of bounds), a bad delay, and whatever setIsInPrison
// throws answer the syntax
void SPrison::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) {
		sendInfo(admin);
		return;
	}

	try {
		runtime::Ptr<Player> playerToPrison = World::getInstance().getPlayer(Util::convertName(params[0]));
		int32_t delay = commons::utils::parseInt(params[1]);
		if (params.size() < 3) // parity: Java's ArrayIndexOutOfBoundsException of params[2], explicit
			throw commons::utils::IndexOutOfBoundsException("params"); // parity: (the same)
		std::string reason = Util::convertName(params[2]);

		for (size_t itr = 3; itr < params.size(); itr++)
			reason += " " + params[itr];

		if (playerToPrison != nullptr) {
			services::PunishmentService::setIsInPrison(*playerToPrison, true, delay, reason);
			PacketSendUtility::sendMessage(admin, "Player " + playerToPrison->getName() + " sent to prison for " + std::to_string(delay) +
				" because " + reason + ".");
		}
	} catch (const std::exception&) { // parity= } catch (Exception e) {
		sendInfo(admin);
	}
}

// Java SPrison.java:48-51
void SPrison::info(Player& player, std::optional<std::string_view> /*message*/) {
	sendInfo(player);
}

// Java SPrison.java:53-55
void SPrison::sendInfo(Player& player) {
	PacketSendUtility::sendMessage(player, "syntax //sprison <player> <delay> <reason>");
}

} // namespace aion::gameserver::handlers::admincommands
