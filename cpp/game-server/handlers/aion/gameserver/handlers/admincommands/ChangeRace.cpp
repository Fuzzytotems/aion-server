#include "aion/gameserver/handlers/admincommands/ChangeRace.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(ChangeRace);

// Java ChangeRace.java:11-13
ChangeRace::ChangeRace() : AdminCommand("changerace", "Switches your race to the opposite faction.") {
}

// Java ChangeRace.java:15-20
void ChangeRace::execute(Player& admin, std::span<const std::string> params) {
	admin.getCommonData()->setRace(admin.getOppositeRace());
	admin.getController().onChangedPlayerAttributes();
	admin.getController().updateNearbyQuests();
}

} // namespace aion::gameserver::handlers::admincommands
