#pragma once

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of a house's relationship crystal ("friendportal"): its owner, his friends and his legion members get the housing friend list page
 * (the client then sends CM_HOUSE_TELEPORT, which checks this AI's name); anyone else STR_HOUSING_TELEPORT_CANT_USE.
 * <p>
 * Java: data/handlers/ai/portals/FriendPortalAI.java, @AIName("friendportal") (the marker is in the .cpp). M5h-2 H2-4, an A1 lease.
 *
 * @author Rolandas
 */
class FriendPortalAI : public gameserver::ai::NpcAI {
public:
	explicit FriendPortalAI(Npc& owner) : gameserver::ai::NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleDialogFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
