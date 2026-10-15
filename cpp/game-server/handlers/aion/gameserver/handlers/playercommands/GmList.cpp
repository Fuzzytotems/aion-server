#include "aion/gameserver/handlers/playercommands/GmList.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/utils/audit/GMService.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(GmList);

GmList::GmList() : PlayerCommand("gmlist", "Lists all available team members.") {
}

// Java GmList.java:22-35
void GmList::execute(Player& player, std::span<const std::string> /*params*/) {
	std::vector<runtime::Ptr<Player>> availableStaffMembers = GMService::getInstance().getAvailableStaffMembers();
	if (availableStaffMembers.empty()) {
		sendInfo(player, "There is no GM online.");
		return;
	}

	std::string sb = "GMs online (" + std::to_string(availableStaffMembers.size()) + "):"; // parity= StringBuilder sb = new StringBuilder("GMs online (" + availableStaffMembers.size() + "):");
	for (const runtime::Ptr<Player>& gm : availableStaffMembers) {
		FriendList::Status status = gm->getFriendList().getStatus();
		sb += "\n\t" + name(*gm) + " (" + commons::utils::StringUtils::toLowerCase(xml::enumName(status)) + ")"; // parity= sb.append("\n\t").append(name(gm)).append(" (").append(status.name().toLowerCase()).append(")");
	}
	sendInfo(player, sb); // parity= sendInfo(player, sb.toString());
}

} // namespace aion::gameserver::handlers::playercommands
