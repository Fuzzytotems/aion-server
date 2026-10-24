#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the thick dust ("thick_dust"): it goes 10 s after its spawn.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/ThickDustAI.java, @AIName("thick_dust") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class ThickDustAI : public NpcAI {
public:
	explicit ThickDustAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
