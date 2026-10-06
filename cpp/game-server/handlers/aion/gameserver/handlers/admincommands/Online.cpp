#include "aion/gameserver/handlers/admincommands/Online.h"

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Online);

Online::Online() : AdminCommand("online", "Shows the number of online players.") {
}

// Java Online.java:20-32
void Online::execute(Player& admin, std::span<const std::string> /*params*/) {
	int32_t elyosCount = 0;
	int32_t asmoCount = 0;
	for (const runtime::Ptr<Player>& player : World::getInstance().getAllPlayers()) {
		if (player->getRace() == Race::ELYOS)
			elyosCount++;
		else
			asmoCount++;
	}
	std::string countInfo = std::to_string(elyosCount + asmoCount) + " (" + std::to_string(elyosCount) + " Elyos / " + std::to_string(asmoCount) +
		" Asmo" + (asmoCount == 1 ? ")" : "s)");
	PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_LIST_USER(countInfo));
}

} // namespace aion::gameserver::handlers::admincommands
