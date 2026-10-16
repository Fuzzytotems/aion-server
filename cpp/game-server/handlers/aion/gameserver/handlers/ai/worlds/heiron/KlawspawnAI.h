#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::heiron {

/**
 * The AI of a Klawspawn in Heiron ("klawspawn"): it does not think and takes 1 damage per hit; while no 212120 is on the map, each attack has a
 * 10 % chance to spawn one where it stands and to make it die by the attacker; its death deletes it.
 * <p>
 * Java: data/handlers/ai/worlds/heiron/KlawspawnAI.java, @AIName("klawspawn") (the marker is in the .cpp).
 *
 * @author cheatkiller
 */
class KlawspawnAI : public GeneralNpcAI {
public:
	explicit KlawspawnAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::heiron
