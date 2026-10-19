#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/OneDmgAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the refuge's drakan ("IDTiamat_2_Drakan_NPC"): full damage (OneDmgAI's base); spawned, it walks to a random player inside.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_DrakanNpcAI.java, @AIName("IDTiamat_2_Drakan_NPC") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class IDTiamat_2_DrakanNpcAI : public OneDmgAI {
public:
	explicit IDTiamat_2_DrakanNpcAI(Npc& owner) : OneDmgAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
