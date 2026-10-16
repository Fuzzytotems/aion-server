#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructAI.h"

#include <cstdint>

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * A gate of Ahserion's Flight ("ahserion_gate"): spawned, it buffs itself with 21515 at a level by npc id (each level 30 s: the Hangar
 * Barricade 40, the Flight Barrier 50, the Bulwark Shield 60) and drops its target.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionGate.java, @AIName("ahserion_gate") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class AhserionGate : public AhserionConstructAI {
public:
	explicit AhserionGate(Npc& owner) : AhserionConstructAI(owner) {}

	void handleSpawned() override;

private:
	void useBuff();

	/** Each level corresponds to 30s buff time. Retail values: extracted from npcs_abyss_monsters.xml */
	int32_t getSkillLevel();
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
