#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of the Steel Rake's suspicious cannons ("suspiciouscannon"): used, it shoots the user off on a fly teleport (247001 from 730769,
 * 73001 from the others).
 * <p>
 * Java: data/handlers/ai/instance/rakes/SuspiciousCannonAI.java, @AIName("suspiciouscannon") (the marker is in the .cpp).
 *
 * @author xTz
 */
class SuspiciousCannonAI : public ActionItemNpcAI {
public:
	explicit SuspiciousCannonAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
