#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of the attackers and bosses of the ascension quests 1006 / 2008 ("ascensationquestnpc": the raider 211042 and orissan 211043, the
 * guardian assassin 205040 and brigade general hellion 205041): an aggressive npc whose every hit deals exactly 1 damage, so the new Daeva
 * cannot die in the fight.
 * <p>
 * Java: data/handlers/ai/quests/AscensationNpcAI.java, @AIName("ascensationquestnpc") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class AscensationNpcAI : public AggressiveNpcAI {
public:
	explicit AscensationNpcAI(Npc& owner) : AggressiveNpcAI(owner) {}

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::quests
