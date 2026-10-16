#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of the central deck's mobile cannon on the Steel Rake ("centralcannon"): used with a 185000052 (else message 1111302), on the Steel
 * Rake it kills the deck's crew (215402-215405, 230727, 231460, 214968) and calls Chief Mate Menekiki (215069).
 * <p>
 * Java: data/handlers/ai/instance/rakes/CentralDeckMobileCannonAI.java, @AIName("centralcannon") (the marker is in the .cpp).
 *
 * @author xTz
 */
class CentralDeckMobileCannonAI : public ActionItemNpcAI {
public:
	explicit CentralDeckMobileCannonAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
