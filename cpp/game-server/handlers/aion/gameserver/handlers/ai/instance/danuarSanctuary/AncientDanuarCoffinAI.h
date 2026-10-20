#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {

/**
 * The Danuar Sanctuaries' ancient coffins ("ancientdanuarcoffin"): no thinking, one damage per hit, a 40 % chance of a mummy at its death.
 * <p>
 * Java: data/handlers/ai/instance/danuarSanctuary/AncientDanuarCoffinAI.java, @AIName("ancientdanuarcoffin") (the marker is in the .cpp).
 *
 * @author Tibald
 */
class AncientDanuarCoffinAI : public GeneralNpcAI {
public:
	explicit AncientDanuarCoffinAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary
