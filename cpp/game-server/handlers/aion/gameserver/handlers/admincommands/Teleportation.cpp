#include "aion/gameserver/handlers/admincommands/Teleportation.h"

#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Teleportation);

// Java Teleportation.java:15-17
Teleportation::Teleportation() : AdminCommand("teleportation", "Toggles teleportation mode when moving via mouse.") {
}

// Java Teleportation.java:19-27
void Teleportation::execute(Player& admin, std::span<const std::string> params) {
	if (admin.isInCustomState(CustomPlayerState::TELEPORTATION_MODE))
		admin.unsetCustomState(CustomPlayerState::TELEPORTATION_MODE);
	else
		admin.setCustomState(CustomPlayerState::TELEPORTATION_MODE);
	sendInfo(admin, "Teleportation mode is now " +
		(admin.isInCustomState(CustomPlayerState::TELEPORTATION_MODE) ? ChatUtil::color("active", utils::JavaColor::GREEN)
		                                                              : ChatUtil::color("inactive", utils::JavaColor::RED)) +
		".");
}

} // namespace aion::gameserver::handlers::admincommands
