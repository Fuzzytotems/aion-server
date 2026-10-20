#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Beritra's portal ("beritra_portal"): used, it ports the player to one of three places in Beritra's arena, protects him and
 * plays the movie 915 a second later.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/BeritraPortalAI.java, @AIName("beritra_portal") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class BeritraPortalAI : public ActionItemNpcAI {
public:
	explicit BeritraPortalAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
