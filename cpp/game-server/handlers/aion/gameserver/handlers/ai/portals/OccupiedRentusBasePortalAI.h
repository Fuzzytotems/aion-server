#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/portals/PortalDialogAI.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of the Occupied Rentus Base portals ("occupy_rentus_portal"): 832991 opens its dialog to Asmodians only, 832992 to Elyos only.
 * <p>
 * Java: data/handlers/ai/portals/OccupiedRentusBasePortalAI.java, @AIName("occupy_rentus_portal") (the marker is in the .cpp).
 *
 * @author Tibald
 */
class OccupiedRentusBasePortalAI : public PortalDialogAI {
public:
	explicit OccupiedRentusBasePortalAI(Npc& owner) : PortalDialogAI(owner) {}

protected:
	void checkDialog(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::portals
