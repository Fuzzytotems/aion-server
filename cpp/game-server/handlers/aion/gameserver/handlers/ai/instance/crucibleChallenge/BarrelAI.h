#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

/**
 * Crucible Challenge's stage 3 barrels ("barrel"): a destroyed barrel spawns its replacement within 4 m and deletes itself.
 * <p>
 * Java: data/handlers/ai/instance/crucibleChallenge/BarrelAI.java, @AIName("barrel") (the marker is in the .cpp).
 *
 * @author xTz
 */
class BarrelAI : public NpcAI {
public:
	explicit BarrelAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
