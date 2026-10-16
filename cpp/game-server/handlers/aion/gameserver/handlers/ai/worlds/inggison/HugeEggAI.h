#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of the huge egg in Inggison ("hugeegg"): it does not think and takes 1 damage per hit; on its death, at even odds, it hatches 217097
 * where it lay and is deleted.
 * <p>
 * Java: data/handlers/ai/worlds/inggison/HugeEggAI.java, @AIName("hugeegg") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class HugeEggAI : public GeneralNpcAI {
public:
	explicit HugeEggAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
