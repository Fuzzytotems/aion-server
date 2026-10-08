#include "aion/gameserver/handlers/admincommands/RPrison.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(RPrison);

RPrison::RPrison() : AdminCommand("rprison") {
}

// Java RPrison.java:23-42
void RPrison::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty() || params.size() > 2) {
		PacketSendUtility::sendMessage(admin, "syntax //rprison <player>");
		return;
	}

	try {
		runtime::Ptr<Player> playerFromPrison = World::getInstance().getPlayer(Util::convertName(params[0]));

		if (playerFromPrison != nullptr) {
			services::PunishmentService::setIsInPrison(*playerFromPrison, false, 0, "");
			PacketSendUtility::sendMessage(admin, "Player " + playerFromPrison->getName() + " removed from prison.");
		}
	} catch (const runtime::NoSuchElementException&) { // parity= } catch (NoSuchElementException nsee) {
		PacketSendUtility::sendMessage(admin, "Usage: //rprison <player>");
	} catch (const std::exception&) { // parity= } catch (Exception e) {
		PacketSendUtility::sendMessage(admin, "Usage: //rprison <player>");
	}
}

// Java RPrison.java:44-47
void RPrison::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //rprison <player>");
}

} // namespace aion::gameserver::handlers::admincommands
