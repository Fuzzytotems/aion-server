#pragma once

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of a house gate ("housegate"), the portal a house-teleport skill summons: its creator, his friends and his group members are asked to
 * move into the creator's house (a return to the battle field kept where the zones allow it). Java's anonymous AIRequest is the callback struct
 * HouseGateAI_AIRequest of the .cpp (fieldmap ai.portals.HouseGateAI$1).
 * <p>
 * Java: data/handlers/ai/portals/HouseGateAI.java, @AIName("housegate") (the marker is in the .cpp). M5h-2 H2-3, an A1 lease.
 *
 * @author xTz, Rolandas
 */
class HouseGateAI : public gameserver::ai::NpcAI {
public:
	explicit HouseGateAI(Npc& owner) : gameserver::ai::NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

private:
	friend struct HouseGateAI_AIRequest;

	/** Java: every zone of player.findZones() canReturnToBattle (the loops of handleDialogStart and acceptRequest) */
	static bool canReturnToBattle(Player& player);
};

} // namespace aion::gameserver::handlers::ai::portals
