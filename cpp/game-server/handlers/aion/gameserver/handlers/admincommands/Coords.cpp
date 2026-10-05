#include "aion/gameserver/handlers/admincommands/Coords.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Coords);

Coords::Coords() : AdminCommand("coords", "Shows the target's current coordinates.") {
}

// Java Coords.java:18-22
void Coords::execute(Player& admin, std::span<const std::string> /*params*/) {
	runtime::Ptr<VisibleObject> target = admin.getTarget() == nullptr ? runtime::Ptr<VisibleObject>(&admin) : admin.getTarget();
	sendInfo(admin, name(*target) + "'s position:\n" +
						commons::utils::StringUtils::replace(target->getPosition()->toCoordString(), ", X:", "\nX:"));
}

} // namespace aion::gameserver::handlers::admincommands
