#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ChestAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of the Steel Rake's key boxes ("steel_rake_key_box", the four Tamer Anikiki leaves): a chest that is deleted 3 minutes after its
 * spawn unless it was opened (dead) or is gone.
 * <p>
 * Java: data/handlers/ai/instance/rakes/SteelRakeKeyBoxAI.java, @AIName("steel_rake_key_box") (the marker is in the .cpp).
 *
 * @author xTz
 */
class SteelRakeKeyBoxAI : public ChestAI {
public:
	explicit SteelRakeKeyBoxAI(Npc& owner) : ChestAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
