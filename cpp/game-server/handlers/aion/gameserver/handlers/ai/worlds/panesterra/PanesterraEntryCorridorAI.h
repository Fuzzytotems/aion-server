#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include <cstdint>

namespace aion::gameserver::handlers::ai::worlds::panesterra {

/**
 * A corridor into Panesterra ("panesterra_entry_corridor"), tied to its fortress by npc id (10111 to 10411): SETPRO1 sends a level-65 player to
 * the event location - during the Beyond Aion Birthday Event always, otherwise when no Panesterra fortress is under siege and his race owns
 * the corridor's fortress.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/PanesterraEntryCorridorAI.java, @AIName("panesterra_entry_corridor") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class PanesterraEntryCorridorAI : public GeneralNpcAI {
private:
	const int32_t relatedFortressId;

public:
	explicit PanesterraEntryCorridorAI(Npc& owner);

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	/** Java: the switch of the constructor */
	static int32_t relatedFortressIdOf(int32_t npcId);

	bool canTeleport(Player& player);

	/** Java: private and unused (its caller is commented out, PanesterraEntryCorridorAI.java:39) */
	void teleportToFortress(Player& player);
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
